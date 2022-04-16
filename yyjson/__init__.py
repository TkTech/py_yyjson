__all__ = [
    'Document',
    'MutableDocument',
    'ReaderFlags',
    'WriterFlags'
]

import enum

from cyyjson import Document, MutableDocument


class ReaderFlags(enum.IntFlag):
    """
    Flags that can be passed into JSON reading functions to control parsing
    behaviour.
    """
    #: Stop when done instead of issues an error if there's additional content
    #: after a JSON document. This option may used to parse small pieces of JSON
    #: in larger data, such as NDJSON.
    STOP_WHEN_DONE = 1 << 1
    #: Allow single trailing comma at the end of an object or array, such as
    #: [1,2,3,] {"a":1,"b":2,}.
    ALLOW_TRAILING_COMMAS = 1 << 2
    #: Allow C-style single line and multiple line comments.
    ALLOW_COMMENTS = 1 << 3
    #: Allow inf/nan number and literal, case-insensitive, such as 1e999, NaN,
    #: inf, -Infinity
    ALLOW_INF_AND_NAN = 1 << 4
    #: Read number as raw string. inf/nan
    #: literal is also read as raw with `ALLOW_INF_AND_NAN` flag.
    NUMBERS_AS_RAW = 1 << 5


class WriterFlags(enum.IntFlag):
    """
    Flags that can be passed into JSON writing functions to control writing
    behaviour.
    """
    #: Write the JSON with 4-space indents and newlines.
    PRETTY = 1 << 0
    #: Escapes unicode as \uXXXXX so taht all output is ASCII.
    ESCAPE_UNICODE = 1 << 1
    #: Escapes / as \/.
    ESCAPE_SLASHES = 1 << 2
    #: Writes Infinity and NaN.
    ALLOW_INF_AND_NAN = 1 << 3
    #: Writes Infinity and NaN as `null` instead of erroring.
    INF_AND_NAN_AS_NULL = 1 << 4
