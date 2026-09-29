#ifndef FFPSD_TESTS_SUPPORT_TEST_SUPPORT_HPP_
#define FFPSD_TESTS_SUPPORT_TEST_SUPPORT_HPP_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ffpsd/ffpsd.hpp>
#include <fstream>
#include <iterator>
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

    // Layer names in those files, UTF-8: "Fon", "Zalivka tsvetom 1", "Fon kopiya", "Urovni 1".
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

    // Every sample distinct enough that a swapped plane or a shifted row shows.
    inline ffpsd::Image Pattern(std::uint32_t width, std::uint32_t height, std::uint16_t channels, std::uint16_t depth = 8)
    {
        ffpsd::Image image;
        image.width = width;
        image.height = height;
        image.channel_count = channels;
        image.depth = depth;
        image.bytes.resize(image.GetSizeBytes());

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

    // A small document with no layers; the size and channels a new one lacks.
    inline ffpsd::Document
    NewDocument(ffpsd::ColorMode color = ffpsd::ColorMode::kRgb, std::uint16_t depth = 8, std::uint32_t width = 4, std::uint32_t height = 3)
    {
        ffpsd::Document doc;
        doc.SetColorMode(color);
        doc.SetDepth(depth);
        doc.SetWidth(width);
        doc.SetHeight(height);
        doc.SetChannelCount(color == ffpsd::ColorMode::kGrayscale ? 1 : 3);
        return doc;
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
