#ifndef FFPSD_TESTS_SUPPORT_TEST_SUPPORT_HPP_
#define FFPSD_TESTS_SUPPORT_TEST_SUPPORT_HPP_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ffpsd/ffpsd.hpp>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ffpsd_test
{
    inline std::string DataFile(const std::string& name)
    {
        return std::string(FFPSD_TEST_DATA_DIR) + "/" + name;
    }

    // Photoshop 2026 files.
    inline const std::string kGrayscalePsd = DataFile("photoshop/grayscale_two_layers.psd");
    inline const std::string kRgbPsd = DataFile("photoshop/rgb_two_layers.psd");
    inline const std::string kRgbLevelsPsd = DataFile("photoshop/rgb_levels.psd");
    inline const std::string kGrayscaleLevelsPsd = DataFile("photoshop/grayscale_two_layers_levels.psd");
    inline const std::string kRgbMasksPsd = DataFile("photoshop/rgb_masks.psd");

    // Layer names in those files, UTF-8: "Fon", "Fon kopiya", "Urovni 1"; and a longer one, "Zalivka tsvetom 1".
    inline const std::string kBackgroundName = "\xD0\xA4\xD0\xBE\xD0\xBD";
    inline const std::string kColorFillName = "\xD0\x97\xD0\xB0\xD0\xBB\xD0\xB8\xD0\xB2\xD0\xBA\xD0\xB0 "
                                              "\xD1\x86\xD0\xB2\xD0\xB5\xD1\x82\xD0\xBE\xD0\xBC 1";
    inline const std::string kBackgroundCopyName = kBackgroundName + " \xD0\xBA\xD0\xBE\xD0\xBF\xD0\xB8\xD1\x8F";
    inline const std::string kLevelsName = "\xD0\xA3\xD1\x80\xD0\xBE\xD0\xB2\xD0\xBD\xD0\xB8 1";

    // Pictures with known pixels.
    inline const std::string kRgbaPng = DataFile("generated/rgba_8bit.png");
    inline const std::string kRgbaWebp = DataFile("generated/rgba_lossless.webp");
    inline const std::string kQuadrantsJpeg = DataFile("generated/rgb_quadrants.jpg");
    inline const std::string kQuadrantsWebp = DataFile("generated/rgb_quadrants.webp");

    // The planes of kRgbaPng and kRgbaWebp: red, green at half alpha, clear blue; white, black, gray at quarter alpha.
    inline const ffpsd::Bytes kRgbaPlanes = {
        255, 0,   0,   255, 0,   128, // R
        0,   255, 0,   255, 0,   128, // G
        0,   0,   255, 255, 0,   128, // B
        255, 128, 0,   255, 255, 64,  // A
    };

    // The same loaded as gray: 0.2126, 0.7152 and 0.0722 of 255, rounded, then the alpha.
    inline const ffpsd::Bytes kRgbaGrayPlanes = {54, 182, 18, 255, 0, 128, 255, 128, 0, 255, 255, 64};

    constexpr std::uint32_t Fourcc(const char (&code)[5])
    {
        const auto byte = [](char value) { return static_cast<std::uint32_t>(static_cast<unsigned char>(value)); };
        return (byte(code[0]) << 24) | (byte(code[1]) << 16) | (byte(code[2]) << 8) | byte(code[3]);
    }

    inline std::uint32_t BigEndianU32(const std::vector<std::uint8_t>& data, std::size_t at = 0)
    {
        const auto byte = [&](std::size_t i) { return static_cast<std::uint32_t>(data.at(at + i)); };
        return (byte(0) << 24) | (byte(1) << 16) | (byte(2) << 8) | byte(3);
    }

    inline std::vector<std::uint8_t> BigEndianBytes(std::uint32_t value)
    {
        const auto byte = [value](int shift) { return static_cast<std::uint8_t>(value >> shift); };
        return {byte(24), byte(16), byte(8), byte(0)};
    }

    inline std::vector<std::uint8_t> Ascii(const char* text)
    {
        return std::vector<std::uint8_t>(text, text + std::strlen(text));
    }

    // Where each copy of what starts in data.
    inline std::vector<std::size_t> Find(const std::vector<std::uint8_t>& data, const std::vector<std::uint8_t>& what)
    {
        std::vector<std::size_t> found;
        for (auto at = data.begin(); (at = std::search(at, data.end(), what.begin(), what.end())) != data.end(); ++at)
            found.push_back(static_cast<std::size_t>(at - data.begin()));
        return found;
    }

    inline std::vector<std::uint8_t> ReadFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    // A file in the test's temporary directory, removed with this; the name is UTF-8.
    class TempFile
    {
    public:
        explicit TempFile(const std::string& name)
            : path_(std::filesystem::path(testing::TempDir()) / std::filesystem::u8path(name))
        {
        }
        TempFile(const TempFile&) = delete;
        TempFile& operator=(const TempFile&) = delete;
        ~TempFile()
        {
            std::error_code ignored;
            std::filesystem::remove(path_, ignored);
        }

        const std::filesystem::path& Path() const noexcept
        {
            return path_;
        }
        std::string Utf8() const
        {
            return path_.u8string();
        }
        void Write(const std::vector<std::uint8_t>& data) const
        {
            std::ofstream(path_, std::ios::binary)
                .write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        }

    private:
        std::filesystem::path path_;
    };

    inline std::uint64_t Fnv1a64(const std::uint8_t* data, std::size_t size)
    {
        std::uint64_t hash = 0xCBF29CE484222325u;
        for (std::size_t i = 0; i < size; ++i)
            hash = (hash ^ data[i]) * 0x100000001B3u;
        return hash;
    }
    inline std::uint64_t Fnv1a64(const std::vector<std::uint8_t>& data)
    {
        return Fnv1a64(data.data(), data.size());
    }

    // The sum of one 8 bit plane, compared with an independent decoder's.
    inline std::uint64_t PlaneSum(const ffpsd::Image& image, std::size_t channel)
    {
        const std::size_t plane = std::size_t{image.width} * image.height;
        std::uint64_t sum = 0;
        for (std::size_t i = 0; i < plane; ++i)
            sum += image.bytes[channel * plane + i];
        return sum;
    }

    // Every sample distinct enough that a swapped plane or a shifted row shows; gray up to two channels, else RGB, unless given.
    inline ffpsd::Image Pattern(
        std::uint32_t width, std::uint32_t height, std::uint16_t channels, std::uint16_t depth = 8,
        std::optional<ffpsd::ColorMode> color_mode = std::nullopt)
    {
        ffpsd::Image image;
        image.width = width;
        image.height = height;
        image.channel_count = channels;
        image.depth = depth;
        image.color_mode = color_mode.value_or(channels <= 2 ? ffpsd::ColorMode::kGrayscale : ffpsd::ColorMode::kRgb);
        image.bytes = ffpsd::Bytes(image.GetSizeBytes());

        const std::size_t samples = image.bytes.size() / image.GetBytesPerSample();
        for (std::size_t i = 0; i < samples; ++i)
        {
            if (depth == 8)
            {
                image.bytes[i] = static_cast<std::uint8_t>(i * 7 + 3);
            }
            else if (depth == 16)
            {
                const auto value = static_cast<std::uint16_t>(i * 2003 + 0x0102);
                std::memcpy(image.bytes.data() + i * 2, &value, 2);
            }
            else
            {
                const float value = static_cast<float>(i) / 16.0f;
                std::memcpy(image.bytes.data() + i * 4, &value, 4);
            }
        }
        return image;
    }

    // An 8 bit image of these planes; gray up to two channels, else RGB, unless given.
    inline ffpsd::Image Planes(
        std::uint32_t width, std::uint32_t height, std::uint16_t channels, ffpsd::Bytes bytes,
        std::optional<ffpsd::ColorMode> color_mode = std::nullopt)
    {
        ffpsd::Image image = Pattern(width, height, channels, 8, color_mode);
        image.bytes = std::move(bytes);
        return image;
    }

    // Every byte differs from its neighbour, so RLE only grows it.
    inline ffpsd::Image Noise(std::uint32_t width, std::uint32_t height, std::uint16_t channels)
    {
        ffpsd::Image image = Pattern(width, height, channels);
        for (std::size_t i = 0; i < image.bytes.size(); ++i)
            image.bytes[i] = static_cast<std::uint8_t>((i * 2654435761u) >> 13);
        return image;
    }

    inline ffpsd::Image Flat(std::uint32_t width, std::uint32_t height, std::uint16_t channels, std::uint8_t value)
    {
        ffpsd::Image image = Pattern(width, height, channels);
        std::fill(image.bytes.begin(), image.bytes.end(), value);
        return image;
    }

    inline std::vector<std::uint16_t> Samples16(const ffpsd::Image& image)
    {
        std::vector<std::uint16_t> samples(image.bytes.size() / 2);
        std::memcpy(samples.data(), image.bytes.data(), image.bytes.size());
        return samples;
    }

    // What no codec takes: nothing, floats, five channels, short bytes, and channels that only look like RGB.
    inline std::vector<ffpsd::Image> ImagesNoCodecTakes()
    {
        ffpsd::Image cut = Pattern(2, 2, 3);
        cut.bytes = ffpsd::Bytes(cut.bytes.data(), cut.bytes.size() - 1);
        return {
            ffpsd::Image(),
            Pattern(2, 2, 3, 32),
            Pattern(2, 2, 5),
            cut,
            Pattern(2, 2, 4, 8, ffpsd::ColorMode::kCmyk),
            Pattern(2, 2, 3, 8, ffpsd::ColorMode::kLab),
        };
    }

    // What GetPixels gives for an image added without transparency: its planes and a fully opaque one.
    inline ffpsd::Image WithOpaqueAlpha(ffpsd::Image image)
    {
        const std::size_t plane = std::size_t{image.width} * image.height * image.GetBytesPerSample();
        const std::size_t start = image.bytes.size();
        ffpsd::Bytes bytes(start + plane, 0xFF);
        std::copy(image.bytes.begin(), image.bytes.end(), bytes.begin());
        const float full = 1.0f;
        for (std::size_t at = start; image.depth == 32 && at < bytes.size(); at += sizeof(full))
            std::memcpy(bytes.data() + at, &full, sizeof(full));
        image.bytes = std::move(bytes);
        ++image.channel_count;
        return image;
    }

    // An 8 bit image with the pixel at x, y moved to where(x, y, width, height).
    template <class Where> ffpsd::Image Moved(const ffpsd::Image& image, bool swap_sides, const Where& where)
    {
        ffpsd::Image result = image;
        if (swap_sides)
            std::swap(result.width, result.height);
        for (std::uint32_t c = 0; c < image.channel_count; ++c)
        {
            for (std::uint32_t y = 0; y < image.height; ++y)
            {
                for (std::uint32_t x = 0; x < image.width; ++x)
                {
                    const auto [to_x, to_y] = where(x, y, image.width, image.height);
                    result.bytes[(c * result.height + to_y) * result.width + to_x] = image.bytes[(c * image.height + y) * image.width + x];
                }
            }
        }
        return result;
    }

    // Every 8 bit pixel a quarter turn clockwise.
    inline ffpsd::Image Clockwise(const ffpsd::Image& image)
    {
        return Moved(image, true, [](std::uint32_t x, std::uint32_t y, std::uint32_t, std::uint32_t h) { return std::pair(h - 1 - y, x); });
    }

    // Every 8 bit pixel mirrored left to right.
    inline ffpsd::Image Mirrored(const ffpsd::Image& image)
    {
        return Moved(
            image, false, [](std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t) { return std::pair(w - 1 - x, y); });
    }

    // Lossy codecs give colors back within this.
    constexpr int kTolerance = 4;

    using Color = std::array<int, 3>;
    constexpr Color kRed = {255, 0, 0};
    constexpr Color kGreen = {0, 255, 0};
    constexpr Color kBlue = {0, 0, 255};
    constexpr Color kWhite = {255, 255, 255};

    inline int SampleAt(const ffpsd::Image& image, std::uint32_t x, std::uint32_t y, std::uint16_t channel = 0)
    {
        return image.bytes[(std::size_t{channel} * image.height + y) * image.width + x];
    }

    inline Color RgbAt(const ffpsd::Image& image, std::uint32_t x, std::uint32_t y)
    {
        return {SampleAt(image, x, y, 0), SampleAt(image, x, y, 1), SampleAt(image, x, y, 2)};
    }

    inline ::testing::AssertionResult Near(const Color& actual, const Color& expected)
    {
        for (std::size_t i = 0; i < 3; ++i)
        {
            if (std::abs(actual[i] - expected[i]) > kTolerance)
                return ::testing::AssertionFailure() << "got " << actual[0] << ", " << actual[1] << ", " << actual[2] << "; expected "
                                                     << expected[0] << ", " << expected[1] << ", " << expected[2];
        }
        return ::testing::AssertionSuccess();
    }

    // Top left, top right, bottom left and bottom right, away from the edges between the quarters.
    inline std::array<Color, 4> Corners(const ffpsd::Image& image)
    {
        const std::uint32_t right = image.width - 3;
        const std::uint32_t bottom = image.height - 3;
        return {RgbAt(image, 2, 2), RgbAt(image, right, 2), RgbAt(image, 2, bottom), RgbAt(image, right, bottom)};
    }

    inline void ExpectCorners(const ffpsd::Image& image, const std::array<Color, 4>& expected, const std::string& what = "")
    {
        const std::array<Color, 4> corners = Corners(image);
        for (std::size_t i = 0; i < 4; ++i)
            EXPECT_TRUE(Near(corners[i], expected[i])) << what << " corner " << i;
    }

    // The pixels of kQuadrantsJpeg and kQuadrantsWebp: 32 x 16, red, green, blue and white quarters.
    inline void ExpectQuadrants(const ffpsd::Image& image, const std::string& what = "")
    {
        ASSERT_EQ(image.width, 32u) << what;
        ASSERT_EQ(image.height, 16u) << what;
        ASSERT_EQ(image.channel_count, 3u) << what;
        ExpectCorners(image, {kRed, kGreen, kBlue, kWhite}, what);
    }

    inline void ExpectRect(const ffpsd::Rect& rect, std::int32_t top, std::int32_t left, std::int32_t bottom, std::int32_t right)
    {
        EXPECT_EQ(rect.top, top);
        EXPECT_EQ(rect.left, left);
        EXPECT_EQ(rect.bottom, bottom);
        EXPECT_EQ(rect.right, right);
    }

    // A Levels record from input_floor to input_ceiling, onto the whole output range at gamma 1.
    inline void ExpectLevelsRecord(
        const ffpsd::LevelsInfo::Channel& record, std::uint16_t input_floor, std::uint16_t input_ceiling, const std::string& what = "")
    {
        EXPECT_EQ(record.input_floor, input_floor) << what;
        EXPECT_EQ(record.input_ceiling, input_ceiling) << what;
        EXPECT_EQ(record.output_floor, 0u) << what;
        EXPECT_EQ(record.output_ceiling, 255u) << what;
        EXPECT_DOUBLE_EQ(record.gamma, 1.0) << what;
    }

    // A small document with no layers.
    inline ffpsd::Document NewDocument(
        ffpsd::ColorMode color_mode = ffpsd::ColorMode::kRgb, std::uint16_t depth = 8, std::uint32_t width = 4, std::uint32_t height = 3)
    {
        return ffpsd::Document(width, height, color_mode, depth);
    }

    inline ffpsd::Document RoundTrip(const ffpsd::Document& doc, ffpsd::Compression compression = ffpsd::Compression::kDefault)
    {
        return ffpsd::Document::Parse(doc.Save(compression));
    }

    inline ffpsd::Bytes Pixels(const ffpsd::Document& doc, std::size_t index)
    {
        return doc.GetLayerByIndex(index)->GetPixels().bytes;
    }

    inline std::uint32_t LayerId(const ffpsd::Layer& layer)
    {
        const ffpsd::TaggedBlock* block = layer.GetTaggedBlockByKey(Fourcc("lyid"));
        return block == nullptr ? 0 : BigEndianU32(block->data);
    }

    inline ffpsd::TaggedBlock Block(const char (&key)[5], std::vector<std::uint8_t> data)
    {
        ffpsd::TaggedBlock block;
        block.key = Fourcc(key);
        block.data = std::move(data);
        return block;
    }

    // Empty when the layer has no such block.
    inline std::vector<std::uint8_t> BlockData(const ffpsd::Layer& layer, const char (&key)[5])
    {
        const ffpsd::TaggedBlock* block = layer.GetTaggedBlockByKey(Fourcc(key));
        return block == nullptr ? std::vector<std::uint8_t>() : block->data;
    }

    // Makes a layer a group marker through the raw door: 1 open, 2 closed, 3 end.
    inline void SetSectionDivider(ffpsd::Layer& layer, std::uint32_t type)
    {
        layer.SetTaggedBlock(Block("lsct", BigEndianBytes(type)));
    }
} // namespace ffpsd_test

#endif // FFPSD_TESTS_SUPPORT_TEST_SUPPORT_HPP_
