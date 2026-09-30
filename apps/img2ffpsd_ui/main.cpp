#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> // turns main into WinMain for a window app on Windows
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_stdlib.h>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#if defined(__APPLE__)
#include <CoreText/CoreText.h>
#endif

namespace fs = std::filesystem;

namespace
{
    constexpr float kFontSize = 18.0f;
    constexpr int kOutput = -1;

    int Processors()
    {
        return static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    }

    struct Settings
    {
        std::vector<std::string> inputs = {"", ""};
        std::string output;
        bool gray = false;
        bool bicubic = false;
        int jobs = Processors(); // not saved: every start has all the processors
    };

    std::string SettingsPath()
    {
        char* folder = SDL_GetPrefPath("ffpsd", "img2ffpsd_ui");
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
                settings.inputs.push_back(value);
            else if (key == "output")
                settings.output = value;
            else if (key == "gray")
                settings.gray = value == "1";
            else if (key == "resize")
                settings.bicubic = value == "bicubic";
        }
        settings.inputs.resize(std::max<std::size_t>(settings.inputs.size(), 2));
        return settings;
    }

    void SaveSettings(const Settings& settings)
    {
        std::string text;
        for (const std::string& input : settings.inputs)
            text += "input=" + input + "\n";
        text += "output=" + settings.output + "\n";
        text += std::string("gray=") + (settings.gray ? "1" : "0") + "\n";
        text += std::string("resize=") + (settings.bicubic ? "bicubic" : "nearest") + "\n";
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
        return target == kOutput ? settings.output : settings.inputs[target];
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

    std::string Label(std::size_t folder)
    {
        return folder == 0 ? "Background" : "Layer " + std::to_string(folder);
    }

    // A folder's pictures: the relative path without the extension, which pairs them, to the file itself.
    using Pictures = std::map<fs::path, fs::path>;

    std::string Extension(const fs::path& path)
    {
        std::string extension = path.extension().u8string();
        for (char& c : extension)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return extension;
    }

    bool IsPicture(const fs::path& path)
    {
        const std::string extension = Extension(path);
        return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
    }

    // What the system leaves behind: Finder's .DS_Store and ._ files, Thumbs.db and the like.
    bool IsHidden(const fs::path& path)
    {
        const std::string name = path.filename().u8string();
        return name.empty() || name[0] == '.' || name == "Thumbs.db" || name == "desktop.ini";
    }

    bool HasVisibleFiles(const fs::path& folder)
    {
        for (auto entry = fs::recursive_directory_iterator(folder); entry != fs::recursive_directory_iterator(); ++entry)
        {
            if (IsHidden(entry->path()))
            {
                if (entry->is_directory())
                    entry.disable_recursion_pending();
            }
            else if (entry->is_regular_file())
            {
                return true;
            }
        }
        return false;
    }

    Pictures FindPictures(const fs::path& folder, std::vector<std::string>& errors)
    {
        std::map<fs::path, std::vector<fs::path>> by_name;
        for (auto entry = fs::recursive_directory_iterator(folder); entry != fs::recursive_directory_iterator(); ++entry)
        {
            if (IsHidden(entry->path()))
            {
                if (entry->is_directory())
                    entry.disable_recursion_pending();
            }
            else if (entry->is_regular_file() && IsPicture(entry->path()))
            {
                const fs::path relative = entry->path().lexically_relative(folder);
                by_name[fs::path(relative).replace_extension()].push_back(relative);
            }
        }

        Pictures pictures;
        for (const auto& [name, files] : by_name)
        {
            if (files.size() == 1)
                pictures[name] = files[0];
            else
                errors.push_back(folder.u8string() + ": " + files[0].u8string() + " and " + files[1].u8string() + " have one name");
        }
        return pictures;
    }

    std::vector<fs::path> OnlyIn(const Pictures& pictures, const Pictures& other)
    {
        std::vector<fs::path> only;
        for (const auto& [name, file] : pictures)
        {
            if (other.count(name) == 0)
                only.push_back(file);
        }
        return only;
    }

    bool IsNumber(const std::string& text)
    {
        return !text.empty() && text.size() < 18 &&
               std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
    }

    // A file named 12 is page 12; one named 12-13 is pages 12 and 13; any other name is no page.
    void AddPages(const std::string& name, std::set<long long>& pages)
    {
        const std::size_t dash = name.find('-');
        if (IsNumber(name))
        {
            pages.insert(std::stoll(name));
        }
        else if (dash != std::string::npos && IsNumber(name.substr(0, dash)) && IsNumber(name.substr(dash + 1)))
        {
            pages.insert(std::stoll(name.substr(0, dash)));
            pages.insert(std::stoll(name.substr(dash + 1)));
        }
    }

    std::vector<std::string> FindGaps(const Pictures& pictures)
    {
        std::map<fs::path, std::set<long long>> pages_by_folder;
        for (const auto& [name, file] : pictures)
            AddPages(name.filename().u8string(), pages_by_folder[name.parent_path()]);

        std::vector<std::string> gaps;
        for (const auto& [folder, pages] : pages_by_folder)
        {
            for (long long page = pages.empty() ? 0 : *pages.begin(); !pages.empty() && page < *pages.rbegin(); ++page)
            {
                if (pages.count(page) == 0)
                    gaps.push_back((folder / std::to_string(page)).u8string());
            }
        }
        return gaps;
    }

    // What one run takes from the settings, and what checking the folders found.
    struct Job
    {
        std::vector<fs::path> folders;
        fs::path output;
        bool gray = false;
        bool bicubic = false;
        int jobs = 1;
        std::vector<Pictures> found;
        std::vector<fs::path> names;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };

    Job Prepare(const Settings& settings)
    {
        Job job;
        job.gray = settings.gray;
        job.bicubic = settings.bicubic;
        job.jobs = settings.jobs;
        for (std::size_t i = 0; i < settings.inputs.size(); ++i)
        {
            job.folders.push_back(fs::u8path(settings.inputs[i]));
            if (settings.inputs[i].empty())
                job.errors.push_back(Label(i) + ": no folder chosen");
            else if (!fs::is_directory(job.folders[i]))
                job.errors.push_back(Label(i) + ": " + settings.inputs[i] + " does not exist");
        }
        job.output = fs::u8path(settings.output);
        if (settings.output.empty())
            job.errors.push_back("Output: no folder chosen");
        if (!job.errors.empty())
            return job;

        for (const fs::path& folder : job.folders)
            job.found.push_back(FindPictures(folder, job.errors));
        const Pictures& bottoms = job.found[0];
        const Pictures& tops = job.found[1];
        for (const fs::path& file : OnlyIn(bottoms, tops))
            job.errors.push_back("Missing in Layer 1: " + file.u8string());
        for (const fs::path& file : OnlyIn(tops, bottoms))
            job.errors.push_back("Missing in Background: " + file.u8string());
        if (bottoms.empty())
            job.errors.push_back("No pictures in " + job.folders[0].u8string());

        for (std::size_t i = 2; i < job.found.size(); ++i)
        {
            for (const fs::path& file : OnlyIn(job.found[i], bottoms))
                job.warnings.push_back("Missing in Background, skipped in " + Label(i) + ": " + file.u8string());
        }
        for (const std::string& gap : FindGaps(bottoms))
            job.warnings.push_back("Possible miss: " + gap);
        if (fs::is_directory(job.output) && HasVisibleFiles(job.output))
            job.warnings.push_back(settings.output + " is not empty: files there may be replaced");

        for (const auto& [name, file] : bottoms)
            job.names.push_back(name);
        return job;
    }

    ffpsd::Image LoadPicture(const fs::path& path, ffpsd::ColorMode color_mode)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("cannot open " + path.u8string());
        const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (Extension(path) == ".png")
            return ffpsd::LoadPng(data.data(), data.size(), color_mode, 8);
        return ffpsd::LoadJpeg(data.data(), data.size(), color_mode, 8);
    }

    void Convert(const Job& job, const fs::path& name)
    {
        const ffpsd::ColorMode color_mode = job.gray ? ffpsd::ColorMode::kGrayscale : ffpsd::ColorMode::kRgb;
        const ffpsd::ResampleFilter filter = job.bicubic ? ffpsd::ResampleFilter::kBicubic : ffpsd::ResampleFilter::kNearest;

        std::vector<ffpsd::Image> images;
        std::vector<std::size_t> folders;
        for (std::size_t i = 0; i < job.folders.size(); ++i)
        {
            const auto picture = job.found[i].find(name);
            if (picture != job.found[i].end())
            {
                images.push_back(LoadPicture(job.folders[i] / picture->second, color_mode));
                folders.push_back(i);
            }
        }

        // The document is as large as the larger of the bottom and the top pictures.
        const ffpsd::Image& bottom = images[0];
        const ffpsd::Image& top = images[1];
        const bool top_is_larger = std::uint64_t{top.width} * top.height >= std::uint64_t{bottom.width} * bottom.height;
        const std::uint32_t width = top_is_larger ? top.width : bottom.width;
        const std::uint32_t height = top_is_larger ? top.height : bottom.height;

        ffpsd::Document doc(width, height, color_mode);
        if (bottom.width == width && bottom.height == height)
        {
            doc.AddBackgroundLayer("Background", bottom);
        }
        else
        {
            doc.AddLayer("Background", bottom)->Resize(width, height, filter);
            doc.SetBackgroundLayer(0);
        }

        for (std::size_t i = 1; i < images.size(); ++i)
        {
            ffpsd::Image& image = images[i];
            image.channel_count = job.gray ? 1 : 3; // layers lose their transparency
            image.bytes.resize(image.GetSizeBytes());
            ffpsd::Layer* layer = doc.AddLayer(Label(folders[i]), image);
            const bool resized = image.width != width || image.height != height;
            if (resized)
                layer->Resize(width, height, filter);

            // The upper layer covers the rest, so it is the composite.
            if (i + 1 == images.size())
                doc.SetMergedImage(resized ? layer->GetPixels() : image);
        }

        fs::path target = job.output / name;
        target += ".psd";
        fs::create_directories(target.parent_path());
        const std::vector<std::uint8_t> bytes = doc.Save();
        std::ofstream file(target, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!file)
            throw std::runtime_error("cannot write " + target.u8string());
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

    // Every thread takes the next file until none are left.
    void ConvertAll(Run& run)
    {
        const auto started = std::chrono::steady_clock::now();
        std::atomic<std::size_t> next{0};
        const auto work = [&] {
            for (std::size_t i = next++; i < run.job.names.size() && !run.cancel; i = next++)
            {
                try
                {
                    Convert(run.job, run.job.names[i]);
                }
                catch (const std::exception& e)
                {
                    const std::lock_guard<std::mutex> lock(run.mutex);
                    run.failures.push_back(run.job.names[i].u8string() + ": " + e.what());
                }
                ++run.done;
            }
        };

        std::vector<std::thread> threads;
        for (int i = 1; i < run.job.jobs; ++i)
            threads.emplace_back(work);
        work();
        for (std::thread& thread : threads)
            thread.join();

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

    enum class Pressed
    {
        kNothing,
        kBrowse,
        kRemove
    };

    // Browse ends every row at the right edge; Remove comes before it on the rows that can go.
    Pressed FolderField(const std::string& label, std::string& folder, bool removable)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label.c_str());
        ImGui::SameLine(ImGui::GetFontSize() * 6);
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
            const Pressed pressed = FolderField(Label(static_cast<std::size_t>(i)), settings.inputs[i], i >= 2);
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
        if (FolderField("Output", settings.output, false) == Pressed::kBrowse)
            OpenFolderDialog(dialog, window, settings, kOutput);
        ImGui::PopID();

        ImGui::Separator();
        ImGui::Checkbox("Grayscale", &settings.gray);
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
