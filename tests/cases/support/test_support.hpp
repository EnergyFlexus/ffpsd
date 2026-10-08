#ifndef FFPSD_TESTS_SUPPORT_TEST_SUPPORT_HPP_
#define FFPSD_TESTS_SUPPORT_TEST_SUPPORT_HPP_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ffpsd/ffpsd.hpp>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ffpsd_test
{
    inline std::string DataFile(const char* name)
    {
        return std::string(FFPSD_TEST_DATA_DIR) + "/" + name;
    }

    // Photoshop 2026 files; see tests/README.md for what each holds.
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

    constexpr std::uint32_t Fourcc(const char (&code)[5])
    {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(code[0])) << 24 |
               static_cast<std::uint32_t>(static_cast<unsigned char>(code[1])) << 16 |
               static_cast<std::uint32_t>(static_cast<unsigned char>(code[2])) << 8 |
               static_cast<std::uint32_t>(static_cast<unsigned char>(code[3]));
    }

    inline std::uint32_t BigEndianU32(const std::vector<std::uint8_t>& data)
    {
        return static_cast<std::uint32_t>(data.at(0)) << 24 | static_cast<std::uint32_t>(data.at(1)) << 16 |
               static_cast<std::uint32_t>(data.at(2)) << 8 | static_cast<std::uint32_t>(data.at(3));
    }

    inline std::vector<std::uint8_t> BigEndianBytes(std::uint32_t value)
    {
        return {
            static_cast<std::uint8_t>(value >> 24), static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 8),
            static_cast<std::uint8_t>(value)};
    }

    inline std::vector<std::uint8_t> ReadFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    // FNV-1a 64, as tests/scripts/dump_psd.py computes it.
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

    // Every byte differs from its neighbour, so RLE only grows it.
    inline ffpsd::Image Noise(std::uint32_t width, std::uint32_t height, std::uint16_t channels)
    {
        ffpsd::Image image = Pattern(width, height, channels);
        for (std::size_t i = 0; i < image.bytes.size(); ++i)
            image.bytes[i] = static_cast<std::uint8_t>((i * 2654435761u) >> 13);
        return image;
    }

    // Every sample the same byte.
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

    // What neither PNG nor JPEG takes: nothing, floats, five channels, short bytes, and channels that only look like RGB.
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
            Pattern(2, 2, 3, 8, ffpsd::ColorMode::kLab)};
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

    // A small document with no layers.
    inline ffpsd::Document NewDocument(
        ffpsd::ColorMode color_mode = ffpsd::ColorMode::kRgb, std::uint16_t depth = 8, std::uint32_t width = 4, std::uint32_t height = 3)
    {
        return ffpsd::Document(width, height, color_mode, depth);
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
        ffpsd::TaggedBlock block;
        block.key = Fourcc("lsct");
        block.data = BigEndianBytes(type);
        layer.SetTaggedBlock(block);
    }
} // namespace ffpsd_test

#endif // FFPSD_TESTS_SUPPORT_TEST_SUPPORT_HPP_
