#ifndef FFPSD_ADJUSTMENTS_HPP_
#define FFPSD_ADJUSTMENTS_HPP_

#include <cstdint>
#include <vector>

namespace ffpsd
{
    // A struct per kind of adjustment layer; kKey is the block that holds its settings.

    // Records not given are the identity, so they leave their channel untouched.
    struct LevelsInfo
    {
        static constexpr std::uint32_t kKey = 0x6C65766C; // 'levl'

        struct Channel
        {
            std::uint16_t input_floor = 0;      // 0 to 253
            std::uint16_t input_ceiling = 255;  // 2 to 255, above the floor
            std::uint16_t output_floor = 0;     // 0 to 255
            std::uint16_t output_ceiling = 255; // 0 to 255
            double gamma = 1.0;                 // 0.1 to 9.99, stored in hundredths
        };

        // [0] is all color channels, [1] channel 0 and so on; a channel gets its own record, then [0].
        std::vector<Channel> channels;
    };
} // namespace ffpsd

#endif // FFPSD_ADJUSTMENTS_HPP_
