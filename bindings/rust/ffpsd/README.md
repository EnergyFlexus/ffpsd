# ffpsd

Rust bindings to [ffpsd](https://github.com/EnergyFlexus/ffpsd), a library that reads and writes Photoshop PSD
and PSB files.

```rust
use ffpsd::{Compression, Document};

fn main() -> ffpsd::Result<()> {
    let mut document = Document::open("in.psd")?;
    for layer in document.layers() {
        println!("{}: {:?}", layer.name(), layer.bounds());
    }
    document.duplicate_layer(document.layer_count() - 1)?.set_name("Copy")?;
    document.save("out.psd", Compression::RleOrRaw)
}
```

## License

MIT
