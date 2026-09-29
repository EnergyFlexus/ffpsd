#include "detail/layer_and_mask/adjustments/levels.hpp"

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"
#include "detail/io/fourcc.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::uint16_t kVersion = 2;
        constexpr std::uint16_t kExtraVersion = 3;
        constexpr std::uint32_t kExtraSignature = Fourcc('L', 'v', 'l', 's');

        // The legacy records, and the total Photoshop writes once 'Lvls' extends them.
        constexpr std::size_t kLegacyRecords = 29;
        constexpr std::size_t kTotalRecords = 62;

        constexpr std::size_t kRecordSize = 10;
        constexpr std::size_t kAlignment = 4;

        LevelsInfo::Channel ReadRecord(BigEndianReader& reader)
        {
            LevelsInfo::Channel channel;
            channel.input_floor = reader.ReadU16();
            channel.input_ceiling = reader.ReadU16();
            channel.output_floor = reader.ReadU16();
            channel.output_ceiling = reader.ReadU16();
            channel.gamma = reader.ReadU16() / 100.0;
            return channel;
        }

        void CheckRecord(const LevelsInfo::Channel& channel, std::size_t index)
        {
            const bool valid = channel.input_floor <= 253 && channel.input_ceiling >= 2 &&
                               channel.input_ceiling <= 255 && channel.input_floor < channel.input_ceiling &&
                               channel.output_floor <= 255 && channel.output_ceiling <= 255 && channel.gamma >= 0.1 &&
                               channel.gamma <= 9.99;
            if (!valid)
                throw std::invalid_argument("ffpsd: Levels record " + std::to_string(index) + " is out of range");
        }

        void WriteRecord(BigEndianWriter& writer, const LevelsInfo::Channel& channel)
        {
            writer.WriteU16(channel.input_floor);
            writer.WriteU16(channel.input_ceiling);
            writer.WriteU16(channel.output_floor);
            writer.WriteU16(channel.output_ceiling);
            writer.WriteU16(static_cast<std::uint16_t>(std::lround(channel.gamma * 100.0)));
        }
    } // namespace

    template <> LevelsInfo ParseAdjustment<LevelsInfo>(const std::vector<std::uint8_t>& data, std::size_t channel_count)
    {
        const std::size_t record_count = channel_count + 1;
        BigEndianReader reader(data);
        const std::uint16_t version = reader.ReadU16();
        if (version != kVersion)
            throw std::runtime_error("ffpsd: Levels version " + std::to_string(version) + " is not supported");

        LevelsInfo levels;
        for (std::size_t i = 0; i < kLegacyRecords; ++i)
            levels.channels.push_back(ReadRecord(reader));

        if (reader.GetRemaining() >= 8 && reader.PeekU32() == kExtraSignature)
        {
            reader.Skip(4);
            reader.ReadU16(); // version 3
            const std::uint16_t total = reader.ReadU16();
            for (std::size_t i = kLegacyRecords; i < total && reader.GetRemaining() >= kRecordSize; ++i)
                levels.channels.push_back(ReadRecord(reader));
        }

        levels.channels.resize(std::min(record_count, levels.channels.size()));
        return levels;
    }

    template <> std::vector<std::uint8_t> EncodeAdjustment<LevelsInfo>(const LevelsInfo& levels)
    {
        for (std::size_t i = 0; i < levels.channels.size(); ++i)
            CheckRecord(levels.channels[i], i);

        const std::size_t total = std::max(kTotalRecords, levels.channels.size());
        const auto record = [&](std::size_t i) {
            return i < levels.channels.size() ? levels.channels[i] : LevelsInfo::Channel();
        };

        BigEndianWriter writer;
        writer.WriteU16(kVersion);
        for (std::size_t i = 0; i < kLegacyRecords; ++i)
            WriteRecord(writer, record(i));

        writer.WriteU32(kExtraSignature);
        writer.WriteU16(kExtraVersion);
        writer.WriteU16(static_cast<std::uint16_t>(total));
        for (std::size_t i = kLegacyRecords; i < total; ++i)
            WriteRecord(writer, record(i));

        // Inside the declared length, unlike the padding of the block itself.
        writer.PadFrom(0, kAlignment);
        return writer.Take();
    }
} // namespace ffpsd::detail
