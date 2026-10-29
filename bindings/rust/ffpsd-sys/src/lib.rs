//! Raw bindings to ffpsd's C API, `ffpsd/c_api.h`; the `ffpsd` crate wraps them in a safe API.
//!
//! The build script links ffpsd statically. It takes, in this order: an installed ffpsd from
//! `FFPSD_DIR`; a build from the sources in this crate with `FFPSD_FROM_SOURCE=1`; the archive of
//! the GitHub release for the target, or the one `FFPSD_ARCHIVE` names; a build from the sources.
//!
//! The C enums are `c_uint` here, as on Linux; MSVC makes them `int`, which passes the same way.
#![allow(non_camel_case_types, non_upper_case_globals)]

// As bindgen formats it, so the check of scripts/generate_rust_bindings.py compares like with like.
#[rustfmt::skip]
mod bindings;

pub use bindings::*;
