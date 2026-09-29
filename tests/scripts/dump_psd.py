"""Dumps every field of a PSD, read from the specification alone, for the expected values of tests/smoke.

Shares no code with ffpsd: planes and blocks are given as their size and FNV-1a 64 hash.
Usage: python tests/scripts/dump_psd.py tests/data/photoshop/rgb_levels.psd
"""
import struct
import sys


def fnv1a64(data):
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


class Reader:
    def __init__(self, data):
        self.data = data
        self.at = 0

    def take(self, n):
        if self.at + n > len(self.data):
            raise ValueError(f'read of {n} at {self.at} past {len(self.data)}')
        chunk = self.data[self.at:self.at + n]
        self.at += n
        return chunk

    def u8(self):
        return self.take(1)[0]

    def u16(self):
        return struct.unpack('>H', self.take(2))[0]

    def i16(self):
        return struct.unpack('>h', self.take(2))[0]

    def u32(self):
        return struct.unpack('>I', self.take(4))[0]

    def i32(self):
        return struct.unpack('>i', self.take(4))[0]

    def fourcc(self):
        return self.take(4).decode('latin-1')


def unpack_bits(data, size):
    out = bytearray()
    i = 0
    while len(out) < size:
        n = data[i]
        i += 1
        if n < 128:
            out += data[i:i + n + 1]
            i += n + 1
        elif n > 128:
            out += bytes([data[i]]) * (257 - n)
            i += 1
    if len(out) != size:
        raise ValueError('a row unpacks past its size')
    return bytes(out)


def decode(reader, rows, row_bytes):
    compression = reader.u16()
    if compression == 0:
        return compression, reader.take(rows * row_bytes)
    if compression != 1:
        raise ValueError(f'compression {compression}')
    counts = [reader.u16() for _ in range(rows)]
    return compression, b''.join(unpack_bits(reader.take(c), row_bytes) for c in counts)


def unicode_string(data):
    count = struct.unpack('>I', data[:4])[0]
    text = data[4:4 + 2 * count].decode('utf-16-be')
    return text.rstrip('\0')


def hexes(data):
    return ' '.join(f'{b:02X}' for b in data)


def main(path):
    data = open(path, 'rb').read()
    r = Reader(data)
    print(f'file {path}: {len(data)} bytes, fnv {fnv1a64(data):#018x}')

    # Section 1
    assert r.take(4) == b'8BPS'
    version = r.u16()
    r.take(6)
    channels, height, width, depth, mode = r.u16(), r.u32(), r.u32(), r.u16(), r.u16()
    print(f'header: version {version}, channels {channels}, {width} x {height}, depth {depth}, mode {mode}')
    assert version == 1 and depth == 8, 'this dumper reads 8 bit PSD only'

    # Section 2
    color_mode_data = r.take(r.u32())
    print(f'color mode data: {len(color_mode_data)} bytes')

    # Section 3
    end = r.u32() + r.at
    print('image resources:')
    while r.at < end:
        assert r.take(4) == b'8BIM'
        rid = r.u16()
        name_length = r.u8()
        name = r.take(name_length).decode('latin-1')
        if (name_length + 1) % 2:
            r.take(1)
        body = r.take(r.u32())
        if len(body) % 2:
            r.take(1)
        extra = ''
        if rid == 1005:
            h, hu, wu, v, vu, htu = struct.unpack('>IhhIhh', body)
            extra = f' horizontal {h:#010x} unit {hu} width unit {wu}, vertical {v:#010x} unit {vu} height unit {htu}'
        elif rid == 1057:
            ver, flag = struct.unpack('>IB', body[:5])
            rest = body[5:]
            writer = unicode_string(rest)
            rest = rest[4 + 2 * struct.unpack('>I', rest[:4])[0]:]
            reader_name = unicode_string(rest)
            rest = rest[4 + 2 * struct.unpack('>I', rest[:4])[0]:]
            extra = f' version {ver}, merged {flag}, writer {writer!r}, reader {reader_name!r}, file version {struct.unpack(">I", rest[:4])[0]}'
        elif len(body) <= 8:
            extra = f' [{hexes(body)}]'
        print(f'  {rid} name {name!r}: {len(body)} bytes, fnv {fnv1a64(body):#018x}{extra}')

    # Section 4
    section_end = r.u32() + r.at
    info_end = r.u32() + r.at
    count = r.i16()
    print(f'layer info: count {count}')
    records = []
    for _ in range(abs(count)):
        top, left, bottom, right = r.i32(), r.i32(), r.i32(), r.i32()
        chans = [(r.i16(), r.u32()) for _ in range(r.u16())]
        assert r.take(4) == b'8BIM'
        blend, opacity, clipping, flags = r.fourcc(), r.u8(), r.u8(), r.u8()
        r.u8()
        extra_end = r.u32() + r.at
        mask = r.take(r.u32())
        ranges = r.take(r.u32())
        name_length = r.u8()
        name = r.take(name_length).decode('latin-1')
        pad = (4 - (name_length + 1) % 4) % 4
        r.take(pad)
        blocks = []
        while r.at < extra_end:
            signature = r.fourcc()
            key = r.fourcc()
            body = r.take(r.u32())
            blocks.append((signature, key, body))
        records.append(dict(bounds=(top, left, bottom, right), chans=chans, blend=blend, opacity=opacity,
                            clipping=clipping, flags=flags, mask=mask, ranges=ranges, name=name, blocks=blocks))

    for i, rec in enumerate(records):
        top, left, bottom, right = rec['bounds']
        print(f'layer {i}: bounds {rec["bounds"]}, blend {rec["blend"]!r}, opacity {rec["opacity"]}, '
              f'clipping {rec["clipping"]}, flags {rec["flags"]:#04x}, legacy name {rec["name"]!r}')
        print(f'  mask data: {len(rec["mask"])} bytes [{hexes(rec["mask"])}]')
        print(f'  blending ranges: {len(rec["ranges"])} bytes, fnv {fnv1a64(rec["ranges"]):#018x}')
        for signature, key, body in rec['blocks']:
            text = ''
            if key == 'luni':
                text = f' name {unicode_string(body)!r}'
            elif key == 'levl':
                # Version 2, then records of input floor, input ceiling, output floor, output ceiling, gamma * 100.
                records = [struct.unpack('>5H', body[2 + 10 * i:12 + 10 * i]) for i in range(4)]
                text = f' version {struct.unpack(">H", body[:2])[0]}, first records {records}'
            elif len(body) <= 8:
                text = f' [{hexes(body)}]'
            print(f'  block {signature} {key!r}: {len(body)} bytes, fnv {fnv1a64(body):#018x}{text}')
        for cid, length in rec['chans']:
            if cid < -1:
                mt, ml, mb, mr = struct.unpack('>iiii', rec['mask'][:16])
                rows, row_bytes = mb - mt, mr - ml
            else:
                rows, row_bytes = bottom - top, right - left
            start = r.at
            compression, plane = decode(r, max(rows, 0), max(row_bytes, 0))
            r.at = start + length
            print(f'  channel {cid}: {length} bytes stored, compression {compression}, '
                  f'{len(plane)} bytes, sum {sum(plane)}, fnv {fnv1a64(plane):#018x}')
    r.at = info_end

    mask_info = r.take(r.u32())
    print(f'global layer mask info: {len(mask_info)} bytes [{hexes(mask_info)}]')
    print('section blocks:')
    while r.at + 12 <= section_end:
        signature = r.fourcc()
        key = r.fourcc()
        body = r.take(r.u32())
        # Padded to 4 after the length.
        r.take((4 - len(body) % 4) % 4 if r.at + (4 - len(body) % 4) % 4 <= section_end else 0)
        print(f'  block {signature} {key!r}: {len(body)} bytes, fnv {fnv1a64(body):#018x}')
    r.at = section_end

    # Section 5
    compression, planes = decode(r, channels * height, width)
    plane = width * height
    print(f'image data: compression {compression}')
    for c in range(channels):
        p = planes[c * plane:(c + 1) * plane]
        print(f'  plane {c}: sum {sum(p)}, fnv {fnv1a64(p):#018x}')
    assert r.at == len(data), f'{len(data) - r.at} bytes left'


if __name__ == '__main__':
    main(sys.argv[1])
