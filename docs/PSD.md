# The PSD file format

What the bytes are and where they sit. Everything in the format is **big
endian**: a 4 byte length is stored most significant byte first.

The reference is Adobe's [Photoshop File Formats Specification](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/).
It is the only normative document, it is incomplete in places, and every
heading below links to the part of it that it describes. Where this file
disagrees with the specification, or the specification disagrees with what
Photoshop writes, it says so.

Sizes written as `4 / 8` mean 4 bytes in a PSD and 8 in a PSB.

## Terminology

The specification's own names, worth knowing because everything else in the
wild abbreviates them:

| Adobe | Common |
|-------|--------|
| File Header Section | header |
| Color Mode Data Section | color mode data |
| Image Resources Section, Image Resource Blocks | resources |
| Layer and Mask Information Section | layer section |
| Additional Layer Information | tagged blocks |
| Image Data Section | composite, merged image |

"Image" in *image resource* means the document as a whole, not pixels.

## What "maximize compatibility" guarantees

A layered PSD stores pixels twice:

* per layer, in the Layer and Mask Information section;
* once more as a flattened composite, in the Image Data section at the end of
  the file.

The composite is what the option controls. With it on, the Image Data section
holds a real flattened image of the whole document. With it off, Photoshop
still writes the section, but its content is a placeholder.

The reliable way to tell the two apart is image resource **1057 (Version
Info)**: its `hasRealMergedData` byte is 1 when the composite is genuine. A
file without resource 1057 predates the flag, and its composite is real.

## File layout

| # | Section | Length prefix |
|---|---------|---------------|
| 1 | File header | fixed 26 bytes |
| 2 | Color mode data | 4 bytes |
| 3 | Image resources | 4 bytes |
| 4 | Layer and mask information | 4 / 8 bytes |
| 5 | Image data (composite) | none, runs to end of file |

Sections 2, 3 and 4 each start with their own length, so anything that only
needs the composite can step over them without looking inside.

## 1. File header, 26 bytes

[File Header Section](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_19840)

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 4 | Signature | always `8BPS` |
| 4 | 2 | Version | 1 = PSD, 2 = PSB |
| 6 | 6 | Reserved | must be zero |
| 12 | 2 | Channels | 1-56, alpha channels included |
| 14 | 4 | Height | rows; max 30000 (PSD), 300000 (PSB) |
| 18 | 4 | Width | columns; same limits |
| 22 | 2 | Depth | bits per channel: 1, 8, 16 or 32 |
| 24 | 2 | Color mode | see below |

Color modes: 0 Bitmap, 1 Grayscale, 2 Indexed, 3 RGB, 4 CMYK, 7 Multichannel,
8 Duotone, 9 Lab.

There is no 24 bit depth: "24 bit RGB" is depth 8 with three channels. The
channel count here belongs to the composite and to the document's alpha
channels; it says nothing about how many channels a layer declares.

## 2. Color mode data

[Color Mode Data Section](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_71638)

```
4 bytes  length
length   data
```

* Indexed: length 768, a palette of 256 RGB entries stored **non
  interleaved** - 256 red bytes, then 256 green, then 256 blue.
* Duotone: an opaque blob. Photoshop itself treats such files as grayscale.
* Everything else: length 0.

## 3. Image resources

[Image Resources Section](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_69883),
[Image Resource Blocks](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_46269),
[Image Resource IDs](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_38034)

Everything about the document that is neither pixels nor layers: resolution,
ICC profile, thumbnail, EXIF, guides, print settings, vector paths. The format
has been extended for thirty years by appending numbered blocks rather than
growing the fixed header, so every block carries its own size and an unknown id
can be stepped over.

```
4 bytes  length of the whole section
```

then blocks until the section is consumed:

```
4 bytes  '8BIM'
2 bytes  resource id
Pascal   name: 1 byte length + bytes, padded to an even total
4 bytes  data size
n bytes  data, padded to an even size
```

Ids live in 1000-1074, with 2000-2997 for path information and 4000-4999 for
plug-in resources - and outside those ranges too: 7000-7999 belongs to Image
Ready and 10000 is the print flags, both of which turn up in ordinary files.
Nothing in the format promises that an id appears once:
whole ranges are handed out, and writers other than Photoshop fill the section
as they please. Photoshop writes the blocks in ascending id order.

### The ids worth interpreting

| ID | Content | Why it matters |
|----|---------|----------------|
| 1005 | ResolutionInfo | dpi, and the units the dialog displays |
| 1006 / 1045 | Alpha channel names, ASCII / Unicode | |
| 1007 / 1077 | Display info | tells a spot channel from an alpha channel |
| 1036 | Thumbnail | JPEG preview, a fast path to a picture |
| 1039 | ICC profile | without it the colors can only be assumed sRGB |
| 1024 / 1026 / 1072 | Layer state, layers group, layer groups enabled | indexed by layer position, see below |
| 1044 | Document specific ids seed | where the next layer id comes from |
| 1069 | Layer selection ids | the selected layers, by `lyid` |
| 1046 | Indexed color table count | how many palette entries are real |
| 1047 | Transparency index | which palette index is transparent |
| 1057 | Version Info | `hasRealMergedData`, and who wrote the file |
| 1064 | Pixel aspect ratio | non-square pixels change the displayed geometry |

### 1005, ResolutionInfo, 16 bytes

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 4 | hRes | Fixed 16.16, **pixels per inch** |
| 4 | 2 | hResUnit | 1 pixels per inch, 2 pixels per cm |
| 6 | 2 | widthUnit | 1 in, 2 cm, 3 pt, 4 picas, 5 columns |
| 8 | 4 | vRes | Fixed 16.16 |
| 12 | 2 | vResUnit | as hResUnit |
| 14 | 2 | heightUnit | as widthUnit |

Both resolutions are in pixels per inch whatever the unit fields say: the four
unit fields only pick how Photoshop displays the numbers in the Image Size
dialog. A file without 1005 is 72 dpi, which is what Photoshop assumes.

Fixed 16.16 is signed: 16 integer bits and 16 fractional ones, so 300 dpi is
`0x012C0000` and the representable maximum is just under 32768.

### 1057, Version Info

```
4 bytes   version of the block, 1
1 byte    hasRealMergedData
Unicode   writer name, the application that wrote the file
Unicode   reader name, the application that can read it
4 bytes   file version
```

Photoshop puts "Adobe Photoshop" in both names. Neither number has anything to
do with the PSD or PSB version in the header, and only version 1 of the block
has a documented layout.

### 1044 is not a layer count

The seed only grows. Delete five layers of ten and the count in section 4
becomes five while the seed stays where it was, because layer ids have to stay
unique for the life of the document: clipping, linking and effects refer to
them.

The specification describes how ids are handed out: "Base value, starting at
which layer IDs will be generated (or a greater value if existing IDs already
exceed it)". So a new id is at least the seed and above every existing `lyid`.

What it does not say is what Photoshop stores afterwards. In practice that is
the last id handed out, not the next one: a file whose layers have ids 1 and 2
stores 2, and one with ids 1 and 5 stores 7 after two more layers came and
went. One past the maximum of the seed and every `lyid` in the file is a new id
under both readings.

### 1024, 1026, 1069 and 1072: the layers panel

These describe layers by their position in the stack, not by id:

* **1024**, layer state information: 2 bytes, the index of the target layer,
  counted from the bottom, as the records are stored;
* **1026**, layers group information: 2 bytes per layer, a group id for layers
  linked to move together (the chain icon), 0 for none. Not the same as a
  layer group;
* **1072**, layer groups enabled id: 1 byte per layer;
* **1069**, layer selection ids: 2 bytes of count, then 4 bytes of `lyid` per
  selected layer.

In practice 1024 and 1069 agree: a target of 1 goes with the `lyid` of the
second record from the bottom. After a layer is added, removed or moved, 1024,
1026 and 1072 describe the wrong layers; 1069 survives a move, since it uses
ids.

### The small ones

* **1046**, indexed color table count: 2 bytes, how many palette entries are
  real;
* **1047**, transparency index: 2 bytes, the palette index that is transparent;
* **1064**, pixel aspect ratio: 4 bytes of version (1 or 2), then an 8 byte
  double, x over y of a pixel.

### 1036, Thumbnail

A small struct followed by JPEG bytes: format (1 = JPEG RGB), width, height,
row bytes, total size, compressed size, then 2 bytes bits per pixel (24) and 2
bytes planes (1).

### Padding, the part that breaks parsers

The pad byte is **not** counted in the size field. Data of size 5 occupies 6
bytes, and the sixth is invisible to the length. The name is padded so that the
length byte and the characters together come out even, which is why an empty
name is two bytes, `0x00 0x00`.

Drift by one byte on the first odd-sized resource and the next `8BIM` check
fails - which is the good outcome; the bad one is a file whose following block
happens to still look valid.

An empty section, a length of 0 and nothing else, is legal.

## 4. Layer and mask information

[Layer and Mask Information Section](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_75067)

```
4 / 8 bytes  section length
```

Inside:

```
4 / 8 bytes  layer info length, rounded up (see below)
2 bytes      layer count, SIGNED
             layer records, one per layer
             channel image data, one blob per channel of every layer
4 bytes      global layer mask info length
             overlay color space (2), color components (8),
             opacity (2, 0 transparent to 100 opaque), kind (1), filler
             tagged blocks until the end of the section
```

The records come first and **all** of them come before any pixels. A layer's
record and its channel data sit in different parts of the section.

The specification says the layer info length is "rounded up to a multiple of
2". In practice it is rounded to 4, with zero bytes at the end. Jumping to the
declared end rather than counting the content is the only way through.

A negative layer count is legal: its absolute value is the number of layers,
and the sign says the first alpha channel of the composite holds the
transparency of the merged result.

**16 and 32 bit documents put the layer records elsewhere.** The layer info
length above is then 0 and the real content sits in the tagged block `Lr16` or
`Lr32`, with the same internal layout. Anything that only reads the classic
location sees "no layers" on every 16 bit file.

### Layer record

[Layer records](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_13084)

| Size | Field | Notes |
|------|-------|-------|
| 4 x 4 | top, left, bottom, right | **signed**, a layer may extend past the canvas |
| 2 | channel count | |
| per channel: 2 + 4/8 | channel id (**signed**) and data length | |
| 4 | blend mode signature | `8BIM` |
| 4 | blend mode key | `norm`, `mul `, `scrn`, `over`, ... note the trailing spaces |
| 1 | opacity | 0-255 |
| 1 | clipping | 0 base, 1 non-base |
| 1 | flags | see below |
| 1 | filler | zero |
| 4 | extra data length | covers everything below |

Flags, quoted: "bit 0 = transparency protected; bit 1 = visible; bit 2 =
obsolete; bit 3 = 1 for Photoshop 5.0 and later, tells if bit 4 has useful
information; bit 4 = pixel data irrelevant to appearance of document".

**Bit 1 is the trap.** The specification names it "visible", but a set bit means
the layer is *hidden* - every implementation reads it that way, and a reader
that trusts the name gets the visibility of every layer backwards.

In practice Photoshop writes `0x08` for an ordinary layer and `0x09` for the
background layer, whose transparency is always protected.

Channel ids: 0, 1, 2 ... are the color channels in color mode order, `-1` is
the transparency mask, `-2` the user layer mask, `-3` the real user mask.

In practice the transparency channel is declared **first**, before the color
channels, and a background layer has no `-1` at all: the channel list belongs to
the layer, not to the document.

### Extra data

```
4 bytes   layer mask data length: 0, 20, 36, or more
          layer mask data          (the spec counts the length field in,
                                    and so says 4, 24 or 40 bytes)
4 bytes   layer blending ranges length
          blending ranges
Pascal    layer name, padded to a multiple of 4 (not 2)
          tagged blocks until the extra data is consumed
```

The Pascal layer name is legacy and cut at 255 bytes. The specification calls
it MacRoman; in practice it is in the code page of the writer's system, so a
Russian Windows writes cp1251. The real name is the `luni` tagged block.

### Layer mask data

[Layer mask / adjustment layer data](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_22582)

```
16 bytes  rectangle: top, left, bottom, right, signed
1 byte    default color, 0 or 255
1 byte    flags
          if the length is 20: 2 bytes of padding
          if the length is 36: 1 byte real flags, 1 byte real background,
                               16 bytes rectangle
```

Mask flags: bit 0 position relative to layer, bit 1 mask disabled, bit 2 invert
(obsolete), bit 3 the mask came from rendering other data, bit 4 the mask has
parameters applied.

When bit 4 is set, mask parameters follow - a flag byte and then any of a
density byte or an 8 byte feather - so the length is **not** one of 0, 20, 36.
The declared length is the only reliable way past this field.

The mask's rectangle is its own. Channel `-2` is measured by it, not by the
layer's rectangle.

### Layer blending ranges

[Layer blending ranges data](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_21332)

```
4 bytes   composite gray blend source: 2 black values, 2 white values
4 bytes   composite gray blend destination
          then per channel: 4 bytes source, 4 bytes destination
```

The number of channel pairs is whatever the declared length holds, and it does
not have to match the layer's channel count. In practice Photoshop writes 40
bytes, five pairs, in grayscale and RGB alike and for layers with and without a
transparency channel; a layer nobody touched has `00 00 FF FF` in every range.

### Additional layer information

[Additional Layer Information](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_71546)

Both at the end of the section and inside a layer's extra data, with the same
layout:

```
4 bytes   '8BIM', or '8B64' in a PSB
4 bytes   key
4 / 8     length
n bytes   data
```

Which keys take an 8 byte length in a PSB is not a property of the block: it is
a fixed list, given in the PSB table below.

**The padding is a multiple of 4, and the specification is wrong about it.** It
says "Length data below, rounded up to an even byte count", while in practice
the payload is padded to a multiple of 4 and the padding is not counted in the
length.

Skipping to an even boundary instead desynchronises on the first payload whose
length is not a multiple of 4, and the failure then lands on the next signature
check rather than on the block that caused it.

Inside a layer's extra data Photoshop pads the payload itself and counts the
padding in the length, so every length there is a multiple of 4: a 3 character
`luni` has 10 bytes of data and a length of 12. Photoshop reads those blocks by
their length alone and calls a file incompatible when a layer block's padding
comes after it. A writer that follows the specification pads to 2 inside the
length, so a reader skips padding only when no signature follows the data.

**Layer level keys.** `lsct` section divider, `luni` Unicode name, `lyid` layer
id, `iOpa` fill opacity (1 byte, and *not* the same as the record's opacity),
`lspf` protection flags, `lclr` the color of the strip in the panel, `lfx2`
and `lrFX` effects, `TySh` and `Txt2` text, `vmsk` and `vsms` vector masks,
`SoCo`, `GdFl`, `PtFl` fill layers, `SoLd`, `SoLE`, `PlLd` smart objects, and
the adjustment layers `brit`, `levl`, `curv`, `hue2`, `blnc`, `mixr`, `nvrt`,
`post`, `thrs`, `grdm`, `selc`.

**The background.** Nothing in the specification says which layer is the
background. In practice Photoshop marks it in three places at once, and marks
no other layer that way:

* flags `0x09`: bit 0, transparency protected;
* `lnsr`, the layer name source, is `bgnd`; an ordinary layer has `layr`, or
  `cont` when it came from another content;
* `lspf` is `0x0000000D`: transparency and position locked, and bit 3, which the
  specification leaves out. An ordinary layer has `0`.

It is always the bottom layer, a document has one at most, it has no `-1`
channel, and it covers the whole canvas at 0, 0. A copy of it is an ordinary
layer: flags `0x08`, `lnsr` of `layr`, `lspf` of `0`, and a `-1` channel of its
own.

The presence of one of the last three groups says the layer is not raster: an
adjustment layer changes the document without pixels of its own, a text layer
keeps its string in the block, a fill layer references a pattern.

**Section level keys.** `Lr16`, `Lr32`, `Layr` the layer structure itself,
`LMsk` the document mask, `Mtrn`, `Mt16`, `Mt32` saved merged transparency,
`FMsk` filter mask, `Patt`, `Pat2`, `Pat3` pattern definitions, `lnkD`, `lnk2`,
`lnk3` the embedded files of linked layers, `artb`, `artd` artboards, `mlst`
animation frames.

### Groups, and the order they are stored in

Layers are stored **bottom to top**, and a group is three or more records:

```
panel                       file, in reading order
[- Group 1]                 lsct type 3   (name "</Layer group>")
   Layer A                  Layer B
   Layer B                  Layer A
                            lsct type 1   (name "Group 1")
```

The boundary record comes *before* the group's contents and the folder record
*after* them. There is no parent pointer anywhere: nesting is recovered by
walking the list and treating the two divider kinds as brackets.

`lsct` holds 4 bytes of type - 0 any other, 1 open folder, 2 closed folder, 3
bounding section divider - optionally followed by `8BIM` and a blend mode key,
optionally followed by a sub type. So the block is 4, 12 or 16 bytes long, and a
group whose blend mode is *pass through* carries `pass` in that key while the
record itself says `norm`.

### Adjustment layers

[Additional Layer Information](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_71546), Adjustment layer (Photoshop 4.0)

An adjustment layer is an ordinary layer record whose settings are a tagged
block: `levl` Levels, `curv` Curves, `hue2` Hue/Saturation, `brit`
Brightness/Contrast, `blnc` Color Balance, `selc` Selective Color, `mixr`
Channel Mixer, `grdm` Gradient Map, `phfl` Photo Filter, `expA` Exposure, `vibA`
Vibrance, `thrs` Threshold, `post` Posterize, `nvrt` Invert, `blwh` Black &
White, `clrL` Color Lookup. It acts on everything below it, through its own mask,
opacity and blend mode.

In practice the record carries no pixels of its own:

* its rectangle is empty, 0 x 0;
* it still declares every channel - transparency, the color channels and the
  layer mask `-2` - and each of them is two bytes, compression 0 and no rows;
* its layer mask data is 20 bytes: an empty rectangle and a default color of
  255, a white mask, so the adjustment applies everywhere;
* its flags are `0x18`: bit 3 as on every layer, and bit 4, pixel data
  irrelevant to the appearance of the document.

### Levels, `levl`

[Additional Layer Information](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_71546), Levels

```
2 bytes   version, 2
29 records of 10 bytes:
  2 bytes   input floor     0 to 253
  2 bytes   input ceiling   2 to 255
  2 bytes   output floor    0 to 255
  2 bytes   output ceiling  0 to 255
  2 bytes   gamma x 100     10 to 999
4 bytes   'Lvls'
2 bytes   version, 3
2 bytes   count of all the records, the 29 above included
          the records past the 29th
```

Record 0 is every color channel at once, the RGB entry of the Levels dialog;
record 1 is channel 0, record 2 channel 1 and so on. The unused ones hold the
identity: 0, 255, 0, 255, gamma 1.

In practice Photoshop writes 62 records in all, so 33 follow `Lvls`, and pads
the data to a multiple of 4 with zeros inside the declared length.

The order matters and the specification does not give it. In practice a channel
goes through its own record first and through record 0 after it: that order
reproduces Photoshop's composite of a file exactly for 99.99% of the pixels, the
other order for 96.5%.

### Channel image data

[Channel image data](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_26431)

Per layer, per channel, in the order the record declared them:

```
2 bytes  compression: 0 raw, 1 RLE, 2 ZIP, 3 ZIP with prediction
         if RLE: 2 (PSD) / 4 (PSB) bytes per row of byte counts, then the rows
```

The declared length in the record **includes** these two bytes and the RLE byte
count table. Nothing inside a blob identifies it: the only way to the next one
is the declared length of the current one, so one wrong length silently skews
every layer after it.

The rows cover the channel's own rectangle. A layer of zero area - a group
divider, or an empty layer - still declares its channels, and each blob is then
just the two bytes of compression.

ZIP with prediction is delta encoded per row for 16 bit data; for 32 bit data
the bytes are also split into planes. Photoshop uses ZIP mostly for 16 and 32
bit layers.

## 5. Image data, the composite

[Image Data Section](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_89817)

```
2 bytes  compression for the whole section
         if RLE: byte counts for every row of every channel, then the data
```

Five things differ from the channel data of a layer:

* the compression method is stored **once** for the entire section;
* the RLE row counts come first for every scan line of every channel, all of
  them, before any pixels. The specification says "the byte counts for all the
  scan lines in the channel", which reads as if each channel carried its own
  table, and that is a common way to get this section wrong;
* the geometry comes from the file header, always the full canvas;
* there are no lengths at all - the section runs to the end of the file;
* the channel order is the color channels in color mode order, then alpha.

Layout is planar: all rows of channel 0, then all rows of channel 1. The
uncompressed size is `channels * height * width * depth / 8`, and for 1 bit
data each row is padded to a whole byte.

This section is what every program that only wants a picture reads, which is
why the negative layer count in section 4 matters here: it says the first alpha
channel of this section is the transparency of the merged result.

## Strings

[Unicode string](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#UnicodeStringDefine)

**Pascal string.** One length byte, then that many characters, then padding.
What it is padded to depends on where it is: an image resource name pads so
that the length byte and the characters together are even, a layer name pads to
a multiple of 4. 255 characters is the ceiling.

The encoding is not portable. It is usually described as MacRoman, but in
practice Photoshop writes the code page of the system it runs on, and nothing in
the file says which one that was. This is why the layer name that matters is the
`luni` block.

**Unicode string.** Quoted: "A 4-byte length field, representing the number of
UTF-16 code units in the string (not bytes). The string of Unicode values, two
bytes per character and a two byte null for the end of the string."

So a terminating `0x0000` belongs to the definition, but in practice it is
there in some blocks and absent in others, and where it is there the count does
not cover it.

The count is what to trust: read that many code units and treat anything left
inside the block as padding. Characters outside the basic plane take two code
units, so the count is not a character count.

## RLE, also known as PackBits

Each row is encoded independently, so a row can be decoded on its own once its
offset is known from the byte count table:

* control byte 0-127: copy the next `n + 1` bytes literally;
* control byte 129-255: repeat the next byte `257 - n` times;
* control byte 128: no operation.

A decoder has to stop when the row's declared byte count is consumed, not when
the output row is full: a corrupt file can claim more.

## PSB differences

| | PSD | PSB |
|---|-----|-----|
| Header version | 1 | 2 |
| Max dimension | 30000 | 300000 |
| Layer/mask section length | 4 bytes | 8 bytes |
| Layer info length | 4 bytes | 8 bytes |
| Channel data length | 4 bytes | 8 bytes |
| RLE row counts | 2 bytes | 4 bytes |
| Tagged block signature | `8BIM` | `8BIM` or `8B64` |
| Tagged block length | 4 bytes | 8 bytes for `LMsk`, `Lr16`, `Lr32`, `Layr`, `Mt16`, `Mt32`, `Mtrn`, `Alph`, `FMsk`, `lnk2`, `FEid`, `FXid`, `PxSD` |

Every difference is a length, and every key on that list holds either data the
size of the canvas or a whole embedded file - which is what needs more than 4
gigabytes when the canvas can be 300000 by 300000.

The list itself is a documentation gap: linked layers have three keys, `lnkD`,
`lnk2` and `lnk3`, all of them holding embedded files, and only `lnk2` is
named. Reading a PSB with the wrong width for a length is also not diagnosable
from the data - the high half of a 64 bit length reads as a plausible small
number, so the block looks empty instead of broken.

## Padding rules in one place

| What | Padded to |
|------|-----------|
| Image resource name | even |
| Image resource data | even |
| Layer info section | multiple of 4 in practice, 2 per the specification |
| Layer name | multiple of 4 |
| Global layer mask info | to its declared length |
| Tagged block data, in section 4 | multiple of 4, not counted in the length |
| Tagged block data, in a layer | multiple of 4, counted in the length |
| 1 bit image data rows | whole bytes |

Padding is why a position and a declared length together are the only safe way
through the file. Reading the fields one understands and assuming the cursor
landed on the next structure works until the first odd size.

## Minimal reader for a compatible file

Getting the flattened image needs no layer parsing at all:

1. Read the 26 byte header, check the signature, the version and the depth.
2. Skip the color mode data by its length.
3. Walk the image resources looking for 1057 (`hasRealMergedData`) and
   optionally 1036 (thumbnail), then jump to the end of the section.
4. Skip the entire layer and mask section by its length.
5. Read the composite: 2 bytes of compression, then the channels, planar.

Only step 3 requires looking inside a section, and only to learn whether the
composite can be trusted.

## Sources

* [Adobe Photoshop File Formats Specification](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/) - the normative
  document, and the source of every quotation above.
* Anchors used in the links are Adobe's own and point inside that single page.

### Where the specification is wrong

Checked against files Photoshop wrote, and marked in place above:

* a tagged block is padded to a multiple of 4, not to an even byte count; in
  section 4 the padding is not counted in the length, in a layer it is;
* the layer info length is rounded to 4 as well;
* the legacy Pascal name is in the code page of the writer's system, not
  MacRoman;
* the transparency channel is declared before the color channels;
* image resource ids appear outside the ranges the specification lists, and so
  do tagged block keys;
* resource 1044 is described as the base for new layer ids, but Photoshop stores
  the last id it handed out.
* the Levels count after `Lvls` is of all the records, not of the ones that
  follow it.
