import enum
from decimal import Decimal
from pathlib import Path
from typing import Any, BinaryIO, Optional, List, Dict, Union, Callable

class SAXHandler:
    """
    Optional base class for :func:`sax` handlers. Subclass it and override
    any subset of the event attributes with methods; events left as ``None``
    are skipped. Subclassing is not required - :func:`sax` accepts any
    object and calls each event method only if present.
    """

    obj_begin: Optional[Callable[[], Any]]
    obj_end: Optional[Callable[[int], Any]]
    arr_begin: Optional[Callable[[], Any]]
    arr_end: Optional[Callable[[int], Any]]
    key: Optional[Callable[[str], Any]]
    string: Optional[Callable[[str], Any]]
    number: Optional[Callable[[Any], Any]]
    boolean: Optional[Callable[[bool], Any]]
    null: Optional[Callable[[], Any]]

class ReaderFlags(enum.IntFlag):
    STOP_WHEN_DONE = 0x02
    ALLOW_TRAILING_COMMAS = 0x04
    ALLOW_COMMENTS = 0x08
    ALLOW_INF_AND_NAN = 0x10
    NUMBERS_AS_RAW = 0x20
    NUMBERS_AS_DECIMAL = 0x20
    BIGNUM_AS_RAW = 0x80
    BIG_NUMBERS_AS_DECIMAL = 0x80
    ALLOW_BOM = 0x100
    ALLOW_EXT_NUMBER = 0x200
    ALLOW_EXT_ESCAPE = 0x400
    ALLOW_EXT_WHITESPACE = 0x800
    ALLOW_SINGLE_QUOTED_STR = 0x1000
    ALLOW_UNQUOTED_KEY = 0x2000
    JSON5 = 0x3E1C

class WriterFlags(enum.IntFlag):
    PRETTY = 0x01
    PRETTY_TWO_SPACES = 0x40
    ESCAPE_UNICODE = 0x02
    ESCAPE_SLASHES = 0x04
    ALLOW_INF_AND_NAN = 0x08
    INF_AND_NAN_AS_NULL = 0x10
    ALLOW_INVALID_UNICODE = 0x20
    WRITE_NEWLINE_AT_END = 0x80
    LOWERCASE_HEX = 0x100
    FP_TO_FLOAT = 0x08000000
    @staticmethod
    def fp_to_fixed(precision: int) -> int: ...

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
        *,
        flags: ReaderFlags = ...,
        default: Optional[Callable[[Any], Any]] = ...,
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
        flags: ReaderFlags = ...,
    ) -> "Document": ...
    def __len__(self) -> int: ...
    def get_pointer(self, pointer: str) -> Any: ...
    def dumps(
        self,
        *,
        flags: WriterFlags = ...,
        at_pointer: str = ...,
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
    def freeze(self) -> "Document": ...
    def thaw(self) -> "Document": ...

def load(fp: Any) -> Any: ...
def loads(s: Union[str, bytes, bytearray, Path]) -> Any: ...
def dumps(obj: Any, *, default: Optional[Callable[[Any], Any]] = None) -> str: ...
def dump(
    obj: Any, fp: Any, *, default: Optional[Callable[[Any], Any]] = None
) -> None: ...
def sax(
    source: Union[bytes, bytearray, memoryview, str, Path, BinaryIO],
    handler: object,
    *,
    flags: ReaderFlags = ...,
    window_size: int = ...,
    max_depth: int = ...,
) -> None: ...
