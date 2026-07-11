import enum
from pathlib import Path
from typing import Any, Optional, List, Dict, Union, Callable

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
    ESCAPE_UNICODE = 0x02
    ESCAPE_SLASHES = 0x04
    ALLOW_INF_AND_NAN = 0x08
    INF_AND_NAN_AS_NULL = 0x10
    WRITE_NEWLINE_AT_END = 0x80

Content = Union[str, bytes, List, Dict, Path]

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
        patch: "Document",
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

def load(
    fp,
    *,
    cls=None,
    object_hook=None,
    parse_float=None,
    parse_int=None,
    parse_constant=None,
    object_pairs_hook=None,
    **kw
): ...
def loads(
    s,
    *,
    cls=None,
    object_hook=None,
    parse_float=None,
    parse_int=None,
    parse_constant=None,
    object_pairs_hook=None,
    **kw
): ...
def dumps(obj: Any, *, default: Optional[Callable[[Any], Any]] = None) -> str: ...
def dump(
    obj: Any, fp: Any, *, default: Optional[Callable[[Any], Any]] = None
) -> None: ...
