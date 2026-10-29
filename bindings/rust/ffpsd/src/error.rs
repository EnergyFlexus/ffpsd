use ffpsd_sys as sys;
use std::ffi::{CStr, CString};
use std::fmt;
use std::path::Path;

/// What kind of failure ffpsd reported.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum ErrorKind {
    InvalidArgument,
    OutOfRange,
    NotFound,
    InvalidFile,
    Io,
    InvalidOperation,
    /// The format is not in this build of ffpsd, or the file uses something ffpsd cannot handle.
    Unsupported,
    OutOfMemory,
    Unknown,
}

/// A failed call: its kind and ffpsd's explanation.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Error {
    kind: ErrorKind,
    message: String,
}

impl Error {
    pub(crate) fn new(kind: ErrorKind, message: impl Into<String>) -> Self {
        Self {
            kind,
            message: message.into(),
        }
    }

    pub fn kind(&self) -> ErrorKind {
        self.kind
    }

    pub fn message(&self) -> &str {
        &self.message
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.message.is_empty() {
            write!(f, "{:?}", self.kind)
        } else {
            f.write_str(&self.message)
        }
    }
}

impl std::error::Error for Error {}

pub type Result<T> = std::result::Result<T, Error>;

/// The error of a failed status, with the message ffpsd left for this thread.
pub(crate) fn check(status: sys::ffpsd_status_t) -> Result<()> {
    let kind = match status {
        sys::FFPSD_STATUS_OK => return Ok(()),
        sys::FFPSD_STATUS_INVALID_ARGUMENT => ErrorKind::InvalidArgument,
        sys::FFPSD_STATUS_OUT_OF_RANGE => ErrorKind::OutOfRange,
        sys::FFPSD_STATUS_NOT_FOUND => ErrorKind::NotFound,
        sys::FFPSD_STATUS_INVALID_FILE => ErrorKind::InvalidFile,
        sys::FFPSD_STATUS_IO => ErrorKind::Io,
        sys::FFPSD_STATUS_INVALID_OPERATION => ErrorKind::InvalidOperation,
        sys::FFPSD_STATUS_UNSUPPORTED => ErrorKind::Unsupported,
        sys::FFPSD_STATUS_OUT_OF_MEMORY => ErrorKind::OutOfMemory,
        _ => ErrorKind::Unknown,
    };
    // SAFETY: ffpsd_last_error returns a null-terminated string that lives until the next call on this thread.
    let message = unsafe { CStr::from_ptr(sys::ffpsd_last_error()) };
    Err(Error::new(kind, message.to_string_lossy()))
}

/// Like check, with NOT_FOUND as Ok(false).
pub(crate) fn check_found(status: sys::ffpsd_status_t) -> Result<bool> {
    if status == sys::FFPSD_STATUS_NOT_FOUND {
        return Ok(false);
    }
    check(status).map(|()| true)
}

pub(crate) fn c_string(text: &str) -> Result<CString> {
    CString::new(text).map_err(|_| Error::new(ErrorKind::InvalidArgument, format!("{text:?} holds a null byte")))
}

/// ffpsd takes paths as UTF-8 on every system.
pub(crate) fn c_path(path: &Path) -> Result<CString> {
    let text = path
        .to_str()
        .ok_or_else(|| Error::new(ErrorKind::InvalidArgument, format!("{} is not UTF-8", path.display())))?;
    c_string(text)
}
