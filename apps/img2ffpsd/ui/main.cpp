#include "common/convert.hpp"
#include "common/pictures.hpp"
#include "common/scan.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> // turns main into WinMain for a window app on Windows
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_stdlib.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <CoreText/CoreText.h>
#endif

namespace fs = std::filesystem;

namespace
{
    constexpr float kFontSize = 18.0f;
    constexpr int kOutput = -1;

    // In the order of img2ffpsd::ColorChoice: what the settings file keeps, and what the list shows.
    const char* const kColorKeys[] = {"auto", "color", "gray"};
    const char* const kColorNames[] = {"Auto", "Color", "Grayscale"};

    int Processors()
    {
        return static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    }

    struct Input
    {
        std::string folder;
        bool visible = true; // the layer it makes; only those above Layer 1 can hide
    };

    struct Settings
    {
        std::vector<Input> inputs = {Input(), Input()};
        std::string output;
        img2ffpsd::ColorChoice color = img2ffpsd::ColorChoice::kAuto;
        bool bicubic = false;
        bool recursive = true;
        int jobs = Processors(); // not saved: every start has all the processors
    };

    std::string SettingsPath()
    {
        char* folder = SDL_GetPrefPath("ffpsd", "img2ffpsd-ui");
        const std::string path = folder != nullptr ? std::string(folder) + "settings.txt" : "settings.txt";
        SDL_free(folder);
        return path;
    }

    Settings LoadSettings()
    {
        Settings settings;
        std::size_t size = 0;
        char* data = static_cast<char*>(SDL_LoadFile(SettingsPath().c_str(), &size));
        if (data == nullptr)
            return settings;

        std::istringstream lines(std::string(data, size));
        SDL_free(data);
        settings.inputs.clear();
        std::string line;
        while (std::getline(lines, line))
        {
            const std::size_t equals = line.find('=');
            if (equals == std::string::npos)
                continue;
            const std::string key = line.substr(0, equals);
            const std::string value = line.substr(equals + 1);
            if (key == "input")
                settings.inputs.push_back(Input{value, true});
            else if (key == "visible" && !settings.inputs.empty())
                settings.inputs.back().visible = value != "false";
            else if (key == "output")
                settings.output = value;
            else if (key == "color")
            {
                for (int i = 0; i < IM_ARRAYSIZE(kColorKeys); ++i)
                {
                    if (value == kColorKeys[i])
                        settings.color = static_cast<img2ffpsd::ColorChoice>(i);
                }
            }
            else if (key == "resize")
                settings.bicubic = value == "bicubic";
            else if (key == "recursive")
                settings.recursive = value != "false";
        }
        settings.inputs.resize(std::max<std::size_t>(settings.inputs.size(), 2));
        return settings;
    }

    void SaveSettings(const Settings& settings)
    {
        std::string text;
        for (const Input& input : settings.inputs)
            text += "input=" + input.folder + "\nvisible=" + (input.visible ? "true" : "false") + "\n";
        text += "output=" + settings.output + "\n";
        text += std::string("color=") + kColorKeys[static_cast<int>(settings.color)] + "\n";
        text += std::string("resize=") + (settings.bicubic ? "bicubic" : "nearest") + "\n";
        text += std::string("recursive=") + (settings.recursive ? "true" : "false") + "\n";
        SDL_SaveFile(SettingsPath().c_str(), text.data(), text.size());
    }

    // SDL may answer from another thread, so the chosen folder waits here for the next frame.
    struct FolderDialog
    {
        std::mutex mutex;
        bool open = false;
        bool done = false;
        int target = 0;
        std::string chosen;
    };

    void OnFolderChosen(void* userdata, const char* const* files, int)
    {
        FolderDialog& dialog = *static_cast<FolderDialog*>(userdata);
        const std::lock_guard<std::mutex> lock(dialog.mutex);
        if (files != nullptr && files[0] != nullptr)
            dialog.chosen = files[0];
        dialog.done = true;
    }

    std::string& Folder(Settings& settings, int target)
    {
        return target == kOutput ? settings.output : settings.inputs[target].folder;
    }

    void OpenFolderDialog(FolderDialog& dialog, SDL_Window* window, Settings& settings, int target)
    {
        dialog.open = true;
        dialog.done = false;
        dialog.target = target;
        dialog.chosen.clear();
        const std::string& current = Folder(settings, target);
        SDL_ShowOpenFolderDialog(OnFolderChosen, &dialog, window, current.empty() ? nullptr : current.c_str(), false);
    }

    void TakeChosenFolder(FolderDialog& dialog, Settings& settings)
    {
        const std::lock_guard<std::mutex> lock(dialog.mutex);
        if (!dialog.done)
            return;
        if (!dialog.chosen.empty())
            Folder(settings, dialog.target) = dialog.chosen;
        dialog.open = false;
        dialog.done = false;
    }

    using img2ffpsd::Pictures;

    // A line per file an issue names, or the issue alone; capitalized, as the window shows sentences.
    void AddLines(const std::vector<img2ffpsd::Issue>& issues, std::vector<std::string>& lines)
    {
        for (const img2ffpsd::Issue& issue : issues)
        {
            std::string text = issue.text;
            text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
            if (issue.items.empty())
                lines.push_back(text);
            for (const std::string& item : issue.items)
                lines.push_back(text + ": " + item);
        }
    }

    // What one run takes from the settings, and what checking the folders found.
    struct Job
    {
        img2ffpsd::Conversion conversion;
        int jobs = 1;
        std::vector<Pictures> found;
        std::vector<fs::path> names;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };

    Job Prepare(const Settings& settings)
    {
        Job job;
        job.conversion.color = settings.color;
        job.conversion.filter = settings.bicubic ? ffpsd::ResampleFilter::kBicubic : ffpsd::ResampleFilter::kNearest;
        job.jobs = settings.jobs;
        for (std::size_t i = 0; i < settings.inputs.size(); ++i)
        {
            const Input& input = settings.inputs[i];
            job.conversion.folders.push_back(fs::u8path(input.folder));
            job.conversion.hidden.push_back(i >= 2 && !input.visible);
            if (input.folder.empty())
                job.errors.push_back(img2ffpsd::LayerName(i) + ": no folder chosen");
        }
        job.conversion.output = fs::u8path(settings.output);
        if (settings.output.empty())
            job.errors.push_back("Output: no folder chosen");
        if (!job.errors.empty())
            return job;

        img2ffpsd::FolderScan scan = img2ffpsd::ScanFolders(job.conversion, settings.recursive);
        AddLines(scan.errors, job.errors);
        AddLines(scan.warnings, job.warnings);
        job.found = std::move(scan.found);
        job.names = std::move(scan.names);
        return job;
    }

    // Shared by the window and the threads that convert.
    struct Run
    {
        Job job;
        std::thread worker;
        std::atomic<std::size_t> done{0};
        std::atomic<bool> cancel{false};
        std::atomic<bool> finished{false};
        std::mutex mutex;
        std::vector<std::string> failures;
        double seconds = 0.0;
    };

    void ConvertAll(Run& run)
    {
        const auto started = std::chrono::steady_clock::now();
        img2ffpsd::ConvertAll(
            run.job.conversion, run.job.found, run.job.names, static_cast<unsigned>(run.job.jobs),
            [&](const fs::path& name, const std::string& error) {
                if (!error.empty())
                {
                    const std::lock_guard<std::mutex> lock(run.mutex);
                    run.failures.push_back(name.u8string() + ": " + error);
                }
                ++run.done;
            },
            &run.cancel);

        run.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        run.finished = true;
    }

    void Begin(Run& run)
    {
        run.worker = std::thread(ConvertAll, std::ref(run));
    }

    void Stop(std::unique_ptr<Run>& run)
    {
        if (run != nullptr && run->worker.joinable())
        {
            run->cancel = true;
            run->worker.join();
        }
        run.reset();
    }

    float ButtonWidth(const char* label)
    {
        return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemSpacing.x;
    }

    // An eye, as Photoshop shows a layer; a stroke across it while hidden. True when clicked.
    bool EyeButton(bool visible)
    {
        const float size = ImGui::GetFrameHeight();
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const bool clicked = ImGui::InvisibleButton("##eye", ImVec2(size, size));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(visible ? "Shown: click to hide the layer" : "Hidden: click to show the layer");

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImU32 color = ImGui::GetColorU32(ImGui::IsItemHovered() ? ImGuiCol_Text : (visible ? ImGuiCol_Text : ImGuiCol_TextDisabled));
        const ImVec2 center(at.x + size * 0.5f, at.y + size * 0.5f);
        const float half = size * 0.36f;
        const float thickness = std::max(1.0f, size * 0.07f);

        // The lids are two arcs through the corners, the pupil a disc between them.
        const float bulge = half * 0.75f;
        const float radius = (half * half + bulge * bulge) / (2.0f * bulge);
        const float angle = std::asin(half / radius);
        const float pi = 3.14159265f;
        draw->PathArcTo(ImVec2(center.x, center.y + radius - bulge), radius, -pi / 2 - angle, -pi / 2 + angle);
        draw->PathStroke(color, ImDrawFlags_None, thickness);
        draw->PathArcTo(ImVec2(center.x, center.y - radius + bulge), radius, pi / 2 - angle, pi / 2 + angle);
        draw->PathStroke(color, ImDrawFlags_None, thickness);
        draw->AddCircleFilled(center, half * 0.32f, color);
        if (!visible)
            draw->AddLine(
                ImVec2(center.x - half, center.y + half * 0.8f), ImVec2(center.x + half, center.y - half * 0.8f), color, thickness);
        return clicked;
    }

    enum class Pressed
    {
        kNothing,
        kBrowse,
        kRemove
    };

    // An eye before the folder on the rows that can go, room for one on the others; Browse ends every row, Remove before it.
    Pressed FolderField(const std::string& label, std::string& folder, bool* visible, bool removable)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label.c_str());
        ImGui::SameLine(ImGui::GetFontSize() * 6);
        if (visible != nullptr)
        {
            if (EyeButton(*visible))
                *visible = !*visible;
        }
        else
        {
            ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ButtonWidth("Browse") - (removable ? ButtonWidth("Remove") : 0.0f));
        ImGui::InputText("##folder", &folder);

        Pressed pressed = Pressed::kNothing;
        if (removable)
        {
            ImGui::SameLine();
            if (ImGui::Button("Remove"))
                pressed = Pressed::kRemove;
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse"))
            pressed = Pressed::kBrowse;
        return pressed;
    }

    ImVec4 Color(unsigned rgb)
    {
        return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, 1.0f);
    }

    void Message(unsigned rgb, const std::string& text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Color(rgb));
        ImGui::TextWrapped("%s", text.c_str());
        ImGui::PopStyleColor();
    }

    float MessagesHeight()
    {
        return ImGui::GetTextLineHeightWithSpacing() * 6;
    }

    void DrawRun(Settings& settings, std::unique_ptr<Run>& run)
    {
        const bool started = run != nullptr && run->worker.joinable();
        const bool converting = started && !run->finished;
        const bool waiting = run != nullptr && !started && run->job.errors.empty() && !run->job.warnings.empty();

        if (converting)
        {
            if (ImGui::Button("Cancel"))
                run->cancel = true;
        }
        else if (waiting)
        {
            if (ImGui::Button("Continue"))
                Begin(*run);
            ImGui::SameLine();
            if (ImGui::Button("Back"))
                run.reset();
        }
        else if (ImGui::Button("Start"))
        {
            SaveSettings(settings);
            Stop(run);
            run = std::make_unique<Run>();
            try
            {
                run->job = Prepare(settings);
            }
            catch (const std::exception& e)
            {
                run->job.errors.push_back(e.what());
            }
            if (run->job.errors.empty() && run->job.warnings.empty())
                Begin(*run);
        }
        if (run == nullptr)
            return;

        if (started)
        {
            const std::size_t total = run->job.names.size();
            const std::size_t done = run->done;
            const std::string count = std::to_string(done) + " / " + std::to_string(total);
            ImGui::SameLine();
            ImGui::ProgressBar(
                total == 0 ? 1.0f : static_cast<float>(done) / static_cast<float>(total), ImVec2(-1.0f, 0.0f), count.c_str());
        }

        const float height = std::max(ImGui::GetContentRegionAvail().y, MessagesHeight());
        ImGui::BeginChild("messages", ImVec2(0.0f, height), ImGuiChildFlags_Borders);
        for (const std::string& error : run->job.errors)
            Message(0xE06C75, error);
        for (const std::string& warning : run->job.warnings)
            Message(0xE5C07B, warning);
        {
            const std::lock_guard<std::mutex> lock(run->mutex);
            for (const std::string& failure : run->failures)
                Message(0xE06C75, failure);
        }
        if (started && run->finished)
        {
            const std::size_t total = run->job.names.size();
            const std::size_t failed = run->failures.size();
            char summary[128];
            std::snprintf(
                summary, sizeof(summary), "%zu of %zu converted in %.1f s%s", run->done - failed, total, run->seconds,
                run->cancel ? ", cancelled" : "");
            Message(failed == 0 && !run->cancel ? 0x98C379 : 0xE06C75, summary);
        }
        ImGui::EndChild();
    }

    // Returns the height the form and the messages under it need.
    float DrawWindow(Settings& settings, FolderDialog& dialog, SDL_Window* window, std::unique_ptr<Run>& run)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin(
            "img2ffpsd", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings);
        const bool busy =
            run != nullptr && (run->worker.joinable() ? !run->finished : !run->job.warnings.empty() && run->job.errors.empty());
        ImGui::BeginDisabled(dialog.open || busy);

        ImGui::TextDisabled("Folders from the bottom layer up, and the one for the PSD files");
        int removed = -1;
        for (int i = 0; i < static_cast<int>(settings.inputs.size()); ++i)
        {
            ImGui::PushID(i);
            Input& input = settings.inputs[static_cast<std::size_t>(i)];
            const Pressed pressed =
                FolderField(img2ffpsd::LayerName(static_cast<std::size_t>(i)), input.folder, i >= 2 ? &input.visible : nullptr, i >= 2);
            if (pressed == Pressed::kBrowse)
                OpenFolderDialog(dialog, window, settings, i);
            else if (pressed == Pressed::kRemove)
                removed = i;
            ImGui::PopID();
        }
        if (removed >= 0)
            settings.inputs.erase(settings.inputs.begin() + removed);
        if (ImGui::Button("Add folder"))
            settings.inputs.emplace_back();

        ImGui::Separator();
        ImGui::PushID(kOutput);
        if (FolderField("Output", settings.output, nullptr, false) == Pressed::kBrowse)
            OpenFolderDialog(dialog, window, settings, kOutput);
        ImGui::PopID();

        ImGui::Separator();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Colors");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
        int color = static_cast<int>(settings.color);
        if (ImGui::Combo("##colors", &color, kColorNames, IM_ARRAYSIZE(kColorNames)))
            settings.color = static_cast<img2ffpsd::ColorChoice>(color);
        ImGui::SameLine();
        ImGui::Checkbox("Recursive", &settings.recursive);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The pictures of the folders inside too");
        ImGui::AlignTextToFramePadding();
        const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
        ImGui::TextUnformatted("Resize");
        ImGui::SameLine();
        if (ImGui::RadioButton("Nearest", !settings.bicubic))
            settings.bicubic = false;
        ImGui::SameLine();
        if (ImGui::RadioButton("Bicubic", settings.bicubic))
            settings.bicubic = true;

        const float slider = ImGui::GetFontSize() * 8;
        ImGui::SameLine(right - slider - ImGui::CalcTextSize("Jobs").x - ImGui::GetStyle().ItemSpacing.x);
        ImGui::TextUnformatted("Jobs");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(slider);
        ImGui::SliderInt("##jobs", &settings.jobs, 1, Processors());
        ImGui::EndDisabled();

        ImGui::Separator();
        DrawRun(settings, run);
        const float needed = ImGui::GetCursorPosY() + (run == nullptr ? MessagesHeight() : 0.0f) + ImGui::GetStyle().WindowPadding.y;
        ImGui::End();
        return needed;
    }

    // Tall enough for the folders it opens with, as far as the screen allows.
    void FitAndShow(SDL_Window* window, float height)
    {
        int width = 0;
        int current = 0;
        SDL_GetWindowSize(window, &width, &current);
        SDL_Rect usable;
        if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(window), &usable))
            height = std::min(height, usable.h * 0.9f);
        SDL_SetWindowSize(window, width, static_cast<int>(height));
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_ShowWindow(window);
    }

    // The colors of Atom One Dark.
    void SetStyle(float scale)
    {
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.FrameRounding = 6.0f;
        style.GrabRounding = 6.0f;
        style.FrameBorderSize = 1.0f;
        style.FramePadding = ImVec2(8.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.WindowPadding = ImVec2(14.0f, 14.0f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = Color(0xABB2BF);
        colors[ImGuiCol_TextDisabled] = Color(0x5C6370);
        colors[ImGuiCol_WindowBg] = Color(0x282C34);
        colors[ImGuiCol_Border] = Color(0x3E4451);
        colors[ImGuiCol_FrameBg] = Color(0x21252B);
        colors[ImGuiCol_FrameBgHovered] = Color(0x2C313A);
        colors[ImGuiCol_FrameBgActive] = Color(0x2C313A);
        colors[ImGuiCol_Button] = Color(0x3E4451);
        colors[ImGuiCol_ButtonHovered] = Color(0x4B5263);
        colors[ImGuiCol_ButtonActive] = Color(0x528BFF);
        colors[ImGuiCol_CheckMark] = Color(0x61AFEF);
        colors[ImGuiCol_SliderGrab] = Color(0x61AFEF);
        colors[ImGuiCol_SliderGrabActive] = Color(0x528BFF);
        colors[ImGuiCol_Separator] = Color(0x3E4451);
        colors[ImGuiCol_TextSelectedBg] = Color(0x3E4451);
        colors[ImGuiCol_PlotHistogram] = Color(0x61AFEF);

        style.ScaleAllSizes(scale);
        style.FontSizeBase = kFontSize;
        style.FontScaleDpi = scale;
    }

    // One font a script, in the order a letter is looked for: Latin and Cyrillic, Chinese, Japanese, Korean.
#if defined(_WIN32)
    const char* const kFonts[] = {"segoeui.ttf", "msyh.ttc", "YuGothM.ttc", "malgun.ttf"};

    std::string FontFile(const char* name)
    {
        const char* windows = SDL_getenv("WINDIR");
        return std::string(windows != nullptr ? windows : "C:/Windows") + "/Fonts/" + name;
    }
#elif defined(__APPLE__)
    const char* const kFonts[] = {"Helvetica", "PingFangSC-Regular", "HiraginoSans-W3", "AppleSDGothicNeo-Regular", "ArialUnicodeMS"};

    // Apple moves its font files between releases, so CoreText is asked where they are.
    std::string FontFile(const char* name)
    {
        std::string path;
        CFStringRef font_name = CFStringCreateWithCString(nullptr, name, kCFStringEncodingUTF8);
        CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithNameAndSize(font_name, 0);
        CFURLRef url = static_cast<CFURLRef>(CTFontDescriptorCopyAttribute(descriptor, kCTFontURLAttribute));
        char buffer[4096];
        if (url != nullptr && CFURLGetFileSystemRepresentation(url, true, reinterpret_cast<UInt8*>(buffer), sizeof(buffer)))
            path = buffer;
        if (url != nullptr)
            CFRelease(url);
        CFRelease(descriptor);
        CFRelease(font_name);
        return path;
    }
#else
    const char* const kFonts[] = {"sans-serif:lang=ru", "sans-serif:lang=zh-cn", "sans-serif:lang=ja", "sans-serif:lang=ko"};

    // Every distribution keeps its fonts elsewhere, so fontconfig is asked which one fits.
    std::string FontFile(const char* pattern)
    {
        std::string path;
        const std::string command = std::string("fc-match -f '%{file}' '") + pattern + "' 2>/dev/null";
        if (FILE* pipe = popen(command.c_str(), "r"))
        {
            char buffer[4096];
            while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr)
                path += buffer;
            pclose(pipe);
        }
        return path;
    }
#endif

    // Every system font found, merged into one; without any, Dear ImGui's own, which has Latin letters only.
    void LoadFont()
    {
        ImFontConfig config;
        std::vector<std::string> added;
        for (const char* font : kFonts)
        {
            const std::string path = FontFile(font);
            if (path.empty() || !SDL_GetPathInfo(path.c_str(), nullptr) || std::find(added.begin(), added.end(), path) != added.end())
                continue;
            ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), kFontSize, &config);
            config.MergeMode = true;
            added.push_back(path);
        }
        if (!config.MergeMode)
        {
            config.SizePixels = kFontSize;
            ImGui::GetIO().Fonts->AddFontDefaultVector(&config);
        }
    }
} // namespace

int main(int, char**)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    const float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_Window* window = SDL_CreateWindow(
        "img2ffpsd", static_cast<int>(620 * scale), static_cast<int>(480 * scale),
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window != nullptr ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr)
    {
        std::fprintf(stderr, "SDL: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // our settings file remembers what matters
    SetStyle(scale);
    LoadFont();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    Settings settings = LoadSettings();
    FolderDialog dialog;
    std::unique_ptr<Run> run;
    bool shown = false;
    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        TakeChosenFolder(dialog, settings);
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        const float needed = DrawWindow(settings, dialog, window, run);
        ImGui::Render();
        if (!shown)
        {
            FitAndShow(window, needed);
            shown = true;
        }

        const ImGuiIO& io = ImGui::GetIO();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, 0x28, 0x2C, 0x34, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }
    SaveSettings(settings);
    Stop(run);

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
