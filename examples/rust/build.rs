// Links against a shared ffpsd installed by `python build.py`, ffpsd-out/ by default.
use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-env-changed=FFPSD_DIR");
    let dir = env::var("FFPSD_DIR").map(PathBuf::from).unwrap_or_else(|_| {
        PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap()).join("../../ffpsd-out")
    });
    let lib = dir.join("lib");

    println!("cargo:rustc-link-search=native={}", lib.display());
    println!("cargo:rustc-link-lib=dylib=ffpsd");

    // Windows finds ffpsd.dll through PATH; elsewhere the program remembers where the library is.
    if env::var("CARGO_CFG_TARGET_OS").as_deref() != Ok("windows") {
        println!("cargo:rustc-link-arg=-Wl,-rpath,{}", lib.display());
    }
}
