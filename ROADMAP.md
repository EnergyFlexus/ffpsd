# Roadmap

What comes next, roughly in this order; no dates. Until 1.0.0 the APIs may still change.

## 1. ZIP compression

Decode and write channels compressed with ZIP and ZIP with prediction, as Photoshop
writes 16 and 32 bit documents and some layers. Today such channels are kept as read,
and reading their pixels throws.

## 2. 16 bit

Every operation at 16 bit as at 8: layers, masks, the composite, resizing, color
conversion, PSB. Today 16 bit is partial.

## 3. CMYK

Read, edit and write CMYK documents: pixels and the composite as Photoshop stores them,
conversion to and from RGB and gray, PNG and JPEG through that conversion. Today CMYK
documents open and keep their layers, but only RGB and gray convert.

## 4. Layer kinds

Tell the kinds of layers apart, at least by the blocks they hold: text, shape, smart
object, fill (solid color, gradient, pattern), artboard, besides the raster, group and
adjustment layers known today. Reading what is inside them comes later.

## 5. Composite of raster layers

Render the picture from the raster layers: visibility, opacity and fill opacity, blend
modes, clipping masks, pixel masks, groups. The composite of a saved document is then
up to date, so viewers other than Photoshop show the edits. Text, effects and
adjustments are not rendered.

## 6. Vector masks

Read a layer's vector mask as paths, keep it right through every change of the canvas
and the layer (moving, resizing, turning, flipping), and render it into the composite.
Today vector masks are kept as read and survive resizing the document, but the other
changes throw on such a layer, as do setting and removing its pixel mask.

## 7. Descriptors

Read and write action descriptors, the typed key and value trees in which Photoshop
keeps text, smart objects, layer effects, fill layers and most adjustments, written
back byte for byte when unchanged. This is what reading those layers rests on. Today
their blocks are kept as raw bytes.
