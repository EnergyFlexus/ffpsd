//! Finds or builds ffpsd and links it; src/lib.rs lists the order of the sources.
use sha2::{Digest, Sha256};
use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

/// The GitHub release whose static archives a build downloads, the one of the crate's version.
const RELEASES: &str = "https://github.com/EnergyFlexus/ffpsd/releases/download";
const RELEASE: &str = concat!("v", env!("CARGO_PKG_VERSION"));

/// The SHA-256 of each archive of RELEASE by Rust target; a target missing here is built from source.
const PREBUILT: &[(&str, &str)] = &[];

/// What CMake reads; Cargo would otherwise rerun the script for any file of the package.
const SOURCES: &[&str] = &[
    "CMakeLists.txt",
    "cmake",
    "ffpsd",
    "vendor/CMakeLists.txt",
    "vendor/libpng",
    "vendor/zlib-ng",
    "vendor/libjpeg-turbo",
    "vendor/libwebp",
];

fn main() {
    for name in ["FFPSD_DIR", "FFPSD_FROM_SOURCE", "FFPSD_ARCHIVE"] {
        println!("cargo:rerun-if-env-changed={name}");
    }
    let root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    for source in SOURCES {
        println!("cargo:rerun-if-changed={}", root.join(source).display());
    }

    // The bindings are generated already, so documentation needs no library.
    if env::var_os("DOCS_RS").is_some() {
        return;
    }

    let target = env::var("TARGET").unwrap();
    let out = PathBuf::from(env::var("OUT_DIR").unwrap());

    let prefix = if let Some(dir) = env::var_os("FFPSD_DIR") {
        PathBuf::from(dir)
    } else if is_set("FFPSD_FROM_SOURCE") {
        build_from_source(&root)
    } else {
        match download(&out, &target) {
            Ok(Some(prefix)) => prefix,
            Ok(None) => build_from_source(&root),
            Err(error) => {
                println!("cargo:warning=ffpsd-sys: {error}; building ffpsd from source instead");
                build_from_source(&root)
            }
        }
    };
    link(&prefix, &target);
}

fn is_set(name: &str) -> bool {
    env::var(name).is_ok_and(|value| matches!(value.as_str(), "1" | "true" | "yes"))
}

/// Static, Release whatever the profile, as Rust links the release C runtime of MSVC even in debug builds.
fn build_from_source(root: &Path) -> PathBuf {
    let format = |feature: &str| {
        if env::var_os(format!("CARGO_FEATURE_{feature}")).is_some() {
            "ON"
        } else {
            "OFF"
        }
    };
    cmake::Config::new(root)
        .profile("Release")
        .define("BUILD_SHARED_LIBS", "OFF")
        .define("CMAKE_INSTALL_LIBDIR", "lib")
        .define("FFPSD_BUILD_APPS", "OFF")
        .define("FFPSD_BUILD_TESTS", "OFF")
        .define("FFPSD_BUILD_EXAMPLES", "OFF")
        .define("FFPSD_BUILD_BENCHMARKS", "OFF")
        .define("FFPSD_WITH_IMGUI", "OFF")
        .define("FFPSD_WITH_PNG", format("PNG"))
        .define("FFPSD_WITH_JPEG", format("JPEG"))
        .define("FFPSD_WITH_WEBP", format("WEBP"))
        .define("FFPSD_MSVC_STATIC_RUNTIME", if static_crt() { "ON" } else { "OFF" })
        .build()
}

fn static_crt() -> bool {
    env::var("CARGO_CFG_TARGET_FEATURE").is_ok_and(|features| features.split(',').any(|f| f == "crt-static"))
}

/// The unpacked archive, or None when there is none for the target.
fn download(out: &Path, target: &str) -> Result<Option<PathBuf>, String> {
    let extension = if target.contains("windows") { "zip" } else { "tar.gz" };
    let (source, sha256) = match env::var("FFPSD_ARCHIVE") {
        Ok(archive) => (archive, None),
        // The archives are built with /MD, which a program with the static C runtime cannot link.
        Err(_) if target.contains("msvc") && static_crt() => return Ok(None),
        Err(_) => match PREBUILT.iter().find(|(triple, _)| *triple == target) {
            Some((_, sha256)) => (
                format!("{RELEASES}/{RELEASE}/ffpsd-static-{RELEASE}-{target}.{extension}"),
                Some(*sha256),
            ),
            None => return Ok(None),
        },
    };

    let archive = out.join(format!("ffpsd.{extension}"));
    if source.contains("://") {
        let status = Command::new("curl")
            .args([
                "--fail",
                "--silent",
                "--show-error",
                "--location",
                "--retry",
                "3",
                "--output",
            ])
            .arg(&archive)
            .arg(&source)
            .status()
            .map_err(|error| format!("cannot run curl: {error}"))?;
        if !status.success() {
            return Err(format!("cannot download {source}"));
        }
    } else {
        fs::copy(&source, &archive).map_err(|error| format!("cannot read {source}: {error}"))?;
    }

    // A wrong hash is a broken or replaced release, which a build from source would hide.
    if let Some(expected) = sha256 {
        let bytes = fs::read(&archive).map_err(|error| format!("cannot read {}: {error}", archive.display()))?;
        let actual: String = Sha256::digest(&bytes)
            .iter()
            .map(|byte| format!("{byte:02x}"))
            .collect();
        if actual != expected {
            panic!("ffpsd-sys: {source} has SHA-256 {actual}, not {expected}");
        }
    }

    // tar of Windows 10 and later reads zip as well.
    let unpacked = out.join("prebuilt");
    let _ = fs::remove_dir_all(&unpacked);
    fs::create_dir_all(&unpacked).map_err(|error| format!("cannot create {}: {error}", unpacked.display()))?;
    let status = Command::new("tar")
        .arg("-xf")
        .arg(&archive)
        .arg("-C")
        .arg(&unpacked)
        .status()
        .map_err(|error| format!("cannot run tar: {error}"))?;
    if !status.success() {
        return Err(format!("cannot unpack {}", archive.display()));
    }

    // The archive holds one folder, named after itself, with the installed library.
    let mut folders = fs::read_dir(&unpacked)
        .map_err(|error| error.to_string())?
        .flatten()
        .map(|entry| entry.path());
    match folders.find(|path| path.join("include").join("ffpsd").is_dir()) {
        Some(prefix) => Ok(Some(prefix)),
        None => Err(format!("{source} holds no installed ffpsd")),
    }
}

/// Static unless the prefix holds a shared ffpsd; a static one brings the C++ standard library along.
fn link(prefix: &Path, target: &str) {
    let lib = ["lib", "lib64"]
        .iter()
        .map(|name| prefix.join(name))
        .find(|dir| dir.is_dir())
        .unwrap_or_else(|| panic!("ffpsd-sys: {} has no lib folder", prefix.display()));
    println!("cargo:rustc-link-search=native={}", lib.display());
    println!("cargo:include={}", prefix.join("include").display());

    // On Windows both kinds have an ffpsd.lib, the DLL's being only its import library.
    let shared = if target.contains("windows") {
        prefix.join("bin").join("ffpsd.dll").is_file()
    } else {
        !lib.join("libffpsd.a").is_file()
    };
    if shared {
        println!("cargo:rustc-link-lib=dylib=ffpsd");
        return;
    }

    println!("cargo:rustc-link-lib=static=ffpsd");
    if target.contains("apple") || target.contains("freebsd") {
        println!("cargo:rustc-link-lib=dylib=c++");
    } else if !target.contains("msvc") {
        println!("cargo:rustc-link-lib=dylib=stdc++");
    }
}
