#ifndef FFPSD_DETAIL_IO_STRINGS_HPP_
#define FFPSD_DETAIL_IO_STRINGS_HPP_

#include "detail/io/big_endian_reader.hpp"
#include "detail/io/big_endian_writer.hpp"

#include <cstddef>
#include <string>

namespace ffpsd::detail
{
    // Length byte plus characters, padded together to a multiple of the alignment.
    constexpr std::size_t kImageResourceNameAlignment = 2;
    constexpr std::size_t kLayerNameAlignment = 4;

    // Not decoded: the bytes are in the writer's system code page.
    std::string ReadPascalString(BigEndianReader& reader, std::size_t alignment);

    // Truncated to 255 bytes at a character boundary, as Photoshop does.
    void WritePascalString(BigEndianWriter& writer, const std::string& text, std::size_t alignment);

    // UTF-16 in, UTF-8 out; one trailing null is dropped, since some blocks count it.
    std::string ReadUnicodeString(BigEndianReader& reader);

    // No trailing null; anything unrepresentable becomes U+FFFD.
    void WriteUnicodeString(BigEndianWriter& writer, const std::string& text);
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_IO_STRINGS_HPP_
