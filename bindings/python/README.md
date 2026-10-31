# ffpsd

Python bindings to [ffpsd](https://github.com/EnergyFlexus/ffpsd), a library that reads and writes Photoshop PSD
and PSB files.

```python
import ffpsd

document = ffpsd.Document.open("in.psd")
for layer in document.layers:
    print(layer.name, layer.bounds)
document.duplicate_layer(-1).name = "Copy"
document.save("out.psd", ffpsd.Compression.RLE_OR_RAW)
```

## License

MIT
