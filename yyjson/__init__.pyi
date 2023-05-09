import enum
from typing import Any, Optional, List, Dict, Union


class ReaderFlags(enum.IntFlag):
    STOP_WHEN_DONE = 0x02
    ALLOW_TRAILING_COMMAS = 0x04
    ALLOW_COMMENTS = 0x08
    ALLOW_INF_AND_NAN = 0x10
    NUMBERS_AS_RAW = 0x20


class WriterFlags(enum.IntFlag):
    PRETTY = 0x01
    ESCAPE_UNICODE = 0x02
    ESCAPE_SLASHES = 0x04
    ALLOW_INF_AND_NAN = 0x08
    INF_AND_NAN_AS_NULL = 0x10


Content = Union[str, bytes, List, Dict]


class Document:
    as_obj: Any
    def __init__(self,
                 content: Content,
                 flags: Optional[ReaderFlags] = ...): ...
    def __len__(self) -> int: ...
    def get_pointer(self, pointer: str) -> Any: ...
    def dumps(self,
              flags: Optional[WriterFlags] = ...,
              at_pointer: Optional[str] = ...
              ) -> str: ...
    def patch(self,
                    patch: 'Document',
                    *,
                    at_pointer: Optional[str] = None,
                    use_merge_patch: bool = False
                    ) -> 'Document': ...
    @property
    def is_mutable(self) -> bool: ...
    def freeze(self) -> None: ...
