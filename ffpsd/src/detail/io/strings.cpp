#include "detail/io/strings.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace ffpsd::detail
{
    namespace
    {
        constexpr std::uint32_t kReplacement = 0xFFFD;
        constexpr std::size_t kMaxPascalLength = 255;

        constexpr std::uint16_t kLeadSurrogateFirst = 0xD800;
        constexpr std::uint16_t kLeadSurrogateLast = 0xDBFF;
        constexpr std::uint16_t kTrailSurrogateFirst = 0xDC00;
        constexpr std::uint16_t kTrailSurrogateLast = 0xDFFF;
        constexpr std::uint32_t kSupplementaryFirst = 0x10000;
        constexpr std::uint32_t kCodePointLast = 0x10FFFF;

        std::size_t PaddingFor(std::size_t size, std::size_t alignment) noexcept
        {
            return alignment > 1 ? (alignment - size % alignment) % alignment : 0;
        }

        bool IsSurrogate(std::uint32_t code) noexcept
        {
            return code >= kLeadSurrogateFirst && code <= kTrailSurrogateLast;
        }

        void AppendUtf8(std::string& text, std::uint32_t code)
        {
            if (code < 0x80)
            {
                text.push_back(static_cast<char>(code));
            }
            else if (code < 0x800)
            {
                text.push_back(static_cast<char>(0xC0 | (code >> 6)));
                text.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else if (code < kSupplementaryFirst)
            {
                text.push_back(static_cast<char>(0xE0 | (code >> 12)));
                text.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                text.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else
            {
                text.push_back(static_cast<char>(0xF0 | (code >> 18)));
                text.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                text.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                text.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
        }

        // Broken input yields U+FFFD and still advances, so the loop cannot spin.
        std::uint32_t NextCodePoint(const std::string& text, std::size_t& at)
        {
            const auto lead = static_cast<unsigned char>(text[at]);

            std::uint32_t code = 0;
            std::size_t extra = 0;
            if (lead < 0x80)
            {
                code = lead;
            }
            else if ((lead & 0xE0) == 0xC0)
            {
                code = lead & 0x1Fu;
                extra = 1;
            }
            else if ((lead & 0xF0) == 0xE0)
            {
                code = lead & 0x0Fu;
                extra = 2;
            }
            else if ((lead & 0xF8) == 0xF0)
            {
                code = lead & 0x07u;
                extra = 3;
            }
            else
            {
                ++at;
                return kReplacement;
            }

            if (at + extra >= text.size())
            {
                at = text.size();
                return kReplacement;
            }

            for (std::size_t i = 1; i <= extra; ++i)
            {
                const auto byte = static_cast<unsigned char>(text[at + i]);
                if ((byte & 0xC0) != 0x80)
                {
                    at += i;
                    return kReplacement;
                }
                code = (code << 6) | (byte & 0x3Fu);
            }

            at += extra + 1;
            return code;
        }
    } // namespace

    std::string ReadPascalString(BigEndianReader& reader, std::size_t alignment)
    {
        const std::uint8_t length = reader.ReadU8();

        std::string text(length, '\0');
        if (length != 0)
            reader.ReadU8Array(reinterpret_cast<std::uint8_t*>(text.data()), length);

        reader.Skip(PaddingFor(1 + text.size(), alignment));
        return text;
    }

    void WritePascalString(BigEndianWriter& writer, const std::string& text, std::size_t alignment)
    {
        std::size_t length = std::min(text.size(), kMaxPascalLength);

        // Back up so the cut does not split a UTF-8 character.
        while (length > 0 && length < text.size() && (static_cast<unsigned char>(text[length]) & 0xC0) == 0x80)
            --length;

        writer.WriteU8(static_cast<std::uint8_t>(length));
        if (length != 0)
            writer.WriteU8Array(reinterpret_cast<const std::uint8_t*>(text.data()), length);

        writer.WriteZeros(PaddingFor(1 + length, alignment));
    }

    std::string ReadUnicodeString(BigEndianReader& reader)
    {
        const std::uint32_t count = reader.ReadU32();
        if (count > reader.GetRemaining() / sizeof(std::uint16_t))
            throw std::runtime_error(
                "ffpsd: unicode string claims " + std::to_string(count) + " characters, only " +
                std::to_string(reader.GetRemaining()) + " bytes left");

        std::vector<std::uint16_t> units(count);
        if (count != 0)
            reader.ReadU16Array(units.data(), units.size());

        if (!units.empty() && units.back() == 0)
            units.pop_back();

        std::string text;
        text.reserve(units.size());
        for (std::size_t i = 0; i < units.size(); ++i)
        {
            std::uint32_t code = units[i];
            const bool has_pair = code >= kLeadSurrogateFirst && code <= kLeadSurrogateLast && i + 1 < units.size() &&
                                  units[i + 1] >= kTrailSurrogateFirst && units[i + 1] <= kTrailSurrogateLast;
            if (has_pair)
            {
                code =
                    kSupplementaryFirst + ((code - kLeadSurrogateFirst) << 10) + (units[i + 1] - kTrailSurrogateFirst);
                ++i;
            }
            else if (IsSurrogate(code))
            {
                // Half of a pair on its own has no meaning outside UTF-16.
                code = kReplacement;
            }

            AppendUtf8(text, code);
        }

        return text;
    }

    void WriteUnicodeString(BigEndianWriter& writer, const std::string& text)
    {
        std::vector<std::uint16_t> units;
        units.reserve(text.size());

        for (std::size_t at = 0; at < text.size();)
        {
            std::uint32_t code = NextCodePoint(text, at);
            if (code > kCodePointLast || IsSurrogate(code))
                code = kReplacement;

            if (code < kSupplementaryFirst)
            {
                units.push_back(static_cast<std::uint16_t>(code));
            }
            else
            {
                code -= kSupplementaryFirst;
                units.push_back(static_cast<std::uint16_t>(kLeadSurrogateFirst + (code >> 10)));
                units.push_back(static_cast<std::uint16_t>(kTrailSurrogateFirst + (code & 0x3FF)));
            }
        }

        writer.WriteU32(static_cast<std::uint32_t>(units.size()));
        if (!units.empty())
            writer.WriteU16Array(units.data(), units.size());
    }
} // namespace ffpsd::detail
