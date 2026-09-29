#!/usr/bin/env python3
"""Writes the PNG test files into tests/data/generated/ with zlib alone, so libpng is checked against an
independent encoder. The pixel values here are the ones tests/png/ expects."""

import struct
import zlib
from pathlib import Path

DATA = Path(__file__).resolve().parent.parent / 'data' / 'generated'


def chunk(kind: bytes, data: bytes) -> bytes:
    body = kind + data
    return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body))


def png(width: int, height: int, bit_depth: int, color_type: int, rows: list, extra: bytes = b'') -> bytes:
    header = struct.pack('>IIBBBBB', width, height, bit_depth, color_type, 0, 0, 0)
    raw = b''.join(b'\x00' + row for row in rows)  # filter type 0 on every row
    return (
        b'\x89PNG\r\n\x1a\n'
        + chunk(b'IHDR', header)
        + extra
        + chunk(b'IDAT', zlib.compress(raw))
        + chunk(b'IEND', b'')
    )


# 3 x 2 RGBA, 8 bit: red, green at half alpha, clear blue; white, black, gray at quarter alpha.
rgba = [
    bytes([255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 0]),
    bytes([255, 255, 255, 255, 0, 0, 0, 255, 128, 128, 128, 64]),
]
(DATA / 'rgba_8bit.png').write_bytes(png(3, 2, 8, 6, rgba))

# 2 x 2 gray, 16 bit.
gray = [struct.pack('>HH', 0x0000, 0xFFFF), struct.pack('>HH', 0x8080, 0x1234)]
(DATA / 'gray_16bit.png').write_bytes(png(2, 2, 16, 0, gray))

# 2 x 1 palette: an opaque and a fully transparent entry.
palette = chunk(b'PLTE', bytes([10, 20, 30, 200, 100, 50])) + chunk(b'tRNS', bytes([255, 0]))
(DATA / 'palette_transparent.png').write_bytes(png(2, 1, 8, 3, [bytes([0, 1])], palette))
