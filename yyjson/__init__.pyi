import enum
from decimal import Decimal
from pathlib import Path
from typing import Any, BinaryIO, Optional, List, Dict, Protocol, Union, Callable

class SAXHandler(Protocol):
    """
    Structural typing for a :func:`sax` handler. Every method is optional; only
    those present are called. A method may return ``False`` to stop parsing
    early.
    """

    def obj_begin(self) -> Any: ...
    def obj_end(self, count: int) -> Any: ...
    def arr_begin(self) -> Any: ...
    def arr_end(self, count: int) -> Any: ...
    def key(self, value: str) -> Any: ...
    def string(self, value: str) -> Any: ...
    def number(self, value: Any) -> Any: ...
    def boolean(self, value: bool) -> Any: ...
    def null(self) -> Any: ...

class ReaderFlags(enum.IntFlag):
    STOP_WHEN_DONE = 0x02
    ALLOW_TRAILING_COMMAS = 0x04
    ALLOW_COMMENTS = 0x08
    ALLOW_INF_AND_NAN = 0x10
    NUMBERS_AS_RAW = 0x20
    NUMBERS_AS_DECIMAL = 0x20
    BIGNUM_AS_RAW = 0x80
    BIG_NUMBERS_AS_DECIMAL = 0x80

class WriterFlags(enum.IntFlag):
    PRETTY = 0x01
    PRETTY_TWO_SPACES = 0x40
    ESCAPE_UNICODE = 0x02
    ESCAPE_SLASHES = 0x04
    ALLOW_INF_AND_NAN = 0x08
    INF_AND_NAN_AS_NULL = 0x10
    WRITE_NEWLINE_AT_END = 0x80

# The constructor either parses (str/bytes/Path) or builds from a Python
# object; with a ``default`` callback the build side can accept anything, so
# this lists the natively-handled types rather than being exhaustive.
Content = Union[
    str, bytes, Path, BinaryIO, Dict, List, tuple, int, float, bool, Decimal, None
]

class Document:
    as_obj: Any
    def __init__(
        self,
        content: Content,
        flags: Optional[ReaderFlags] = ...,
        default: Callable[[Any], Any] = ...,
    ): ...
    @classmethod
    def from_obj(
        cls,
        obj: Any,
        *,
        default: Optional[Callable[[Any], Any]] = None,
    ) -> "Document": ...
    @classmethod
    def from_json(
        cls,
        content: Union[str, bytes, Path],
        *,
        flags: Optional[ReaderFlags] = ...,
    ) -> "Document": ...
    def __len__(self) -> int: ...
    def get_pointer(self, pointer: str) -> Any: ...
    def dumps(
        self,
        flags: Optional[WriterFlags] = ...,
        at_pointer: Optional[str] = ...,
    ) -> str: ...
    def patch(
        self,
        patch: Union["Document", Content],
        *,
        at_pointer: Optional[str] = None,
        use_merge_patch: bool = False
    ) -> "Document": ...
    @property
    def is_thawed(self) -> bool: ...
    @property
    def bytes_read(self) -> int: ...
    def freeze(self) -> None: ...
    def thaw(self) -> None: ...

def load(fp: Any) -> Any: ...
def loads(s: Union[str, bytes, Path]) -> Any: ...
def dumps(obj: Any, *, default: Optional[Callable[[Any], Any]] = None) -> str: ...
def dump(
    obj: Any, fp: Any, *, default: Optional[Callable[[Any], Any]] = None
) -> None: ...
def sax(
    source: Union[bytes, str, Path, BinaryIO],
    handler: SAXHandler,
    *,
    flags: Optional[ReaderFlags] = ...,
    window_size: int = ...,
    max_depth: int = ...,
) -> None: ...
