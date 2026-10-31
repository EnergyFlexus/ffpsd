"""What ffpsd raises; each error is also the built-in exception Python code would catch for it."""


class Error(Exception):
    """Any error of ffpsd."""


class InvalidArgumentError(Error, ValueError):
    """An argument ffpsd cannot take: a size, a mode, the shape or type of pixels."""


class OutOfRangeError(Error, IndexError):
    """An index past the end, such as a layer the document does not have."""


class InvalidFileError(Error, ValueError):
    """Data that is not a valid PSD, PSB or picture."""


class IoError(Error, OSError):
    """A file that cannot be opened, read or written."""


class InvalidOperationError(Error, RuntimeError):
    """A call this document or layer does not allow, such as one on a layer removed from its document."""


class UnsupportedError(Error, NotImplementedError):
    """A format this build of ffpsd lacks."""
