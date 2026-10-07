#include "detail/io/compression.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::size_t kMaxChunk = 128;
        constexpr std::size_t kMinRun = 3;

        // -128 is a no-op: some writers use it as padding.
        constexpr std::int8_t kNoOp = -128;
    } // namespace

    void UnpackBits(const std::uint8_t* data, std::size_t size, std::uint8_t* out, std::size_t out_size)
    {
        // 16 byte steps are plain stores, cheaper than a call per short chunk; they run only where their overrun stays in the row.
        constexpr std::size_t kStep = 16;
        std::size_t in = 0;
        std::size_t written = 0;
        while (written < out_size)
        {
            if (in >= size)
                throw std::runtime_error(
                    "ffpsd: PackBits data ends after " + std::to_string(written) + " of " + std::to_string(out_size) + " bytes");

            const auto header = static_cast<std::int8_t>(data[in++]);
            if (header >= 0)
            {
                const std::size_t count = static_cast<std::size_t>(header) + 1;
                if (count > size - in || count > out_size - written)
                    throw std::runtime_error("ffpsd: PackBits literal runs past its row");

                if (size - in >= count + kStep && out_size - written >= count + kStep)
                {
                    for (std::size_t step = 0; step < count; step += kStep)
                        std::memcpy(out + written + step, data + in + step, kStep);
                }
                else
                {
                    std::memcpy(out + written, data + in, count);
                }
                in += count;
                written += count;
            }
            else if (header != kNoOp)
            {
                const std::size_t count = static_cast<std::size_t>(1 - header);
                if (in >= size || count > out_size - written)
                    throw std::runtime_error("ffpsd: PackBits run runs past its row");

                const std::uint8_t value = data[in++];
                if (out_size - written >= count + kStep)
                {
                    for (std::size_t step = 0; step < count; step += kStep)
                        std::memset(out + written + step, value, kStep);
                }
                else
                {
                    std::memset(out + written, value, count);
                }
                written += count;
            }
        }
    }

    void PackBits(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out)
    {
        std::size_t at = 0;
        while (at < size)
        {
            std::size_t run = 1;
            while (at + run < size && run < kMaxChunk && data[at + run] == data[at])
                ++run;

            if (run >= kMinRun)
            {
                out.push_back(static_cast<std::uint8_t>(1 - static_cast<int>(run)));
                out.push_back(data[at]);
                at += run;
                continue;
            }

            // A literal stops where a run worth encoding starts.
            const std::size_t start = at;
            while (at < size && at - start < kMaxChunk)
            {
                if (at + 2 < size && data[at] == data[at + 1] && data[at] == data[at + 2])
                    break;
                ++at;
            }

            out.push_back(static_cast<std::uint8_t>(at - start - 1));
            out.insert(out.end(), data + start, data + at);
        }
    }
} // namespace ffpsd::detail
