__all__ = ["Document", "ReaderFlags", "WriterFlags"]

import enum

from cyyjson import Document

READER_RAW_AS_DECIMAL = 0x100

class ReaderFlags(enum.IntFlag):
    """
    Flags that can be passed into JSON reading functions to control parsing
    behaviour.
    """

    #: Stop when done instead of issues an error if there's additional content
    #: after a JSON document. This option may be used to parse small pieces of
    #: JSON in larger data, such as NDJSON.
    STOP_WHEN_DONE = 0x02
    #: Allow single trailing comma at the end of an object or array, such as
    #: [1,2,3,] {"a":1,"b":2,}.
    ALLOW_TRAILING_COMMAS = 0x04
    #: Allow C-style single line and multiple line comments.
    ALLOW_COMMENTS = 0x08
    #: Allow inf/nan number and literal, case-insensitive, such as 1e999, NaN,
    #: inf, -Infinity
    ALLOW_INF_AND_NAN = 0x10
    #: Read all numbers as Python long integers.
    NUMBERS_AS_RAW = 0x20
    #: Read all numbers as Decimal objects instead of native types. This option
    #: is useful for preserving the exact precision of numbers or for handling
    #: numbers that are too large to fit in a native type.
    NUMBERS_AS_DECIMAL = 0x20 | READER_RAW_AS_DECIMAL
    #: Read big numbers as Python long integers.
    BIGNUM_AS_RAW = 0x80
    #: Like `NUMBERS_AS_DECIMAL`, but only for numbers that are too large to
    #: fit in a native type.
    BIG_NUMBERS_AS_DECIMAL = 0x80 | READER_RAW_AS_DECIMAL


class WriterFlags(enum.IntFlag):
    """
    Flags that can be passed into JSON writing functions to control writing
    behaviour.
    """

    #: Write the JSON with 4-space indents and newlines.
    PRETTY = 0x01
    #: Write JSON pretty with 2 space indent. This flag will override
    #: the PRETTY flag.
    PRETTY_TWO_SPACES = 0x40
    #: Escapes unicode as \uXXXXX so that all output is ASCII.
    ESCAPE_UNICODE = 0x02
    #: Escapes / as \\/.
    ESCAPE_SLASHES = 0x04
    #: Writes Infinity and NaN.
    ALLOW_INF_AND_NAN = 0x08
    #: Writes Infinity and NaN as `null` instead of raising an error.
    INF_AND_NAN_AS_NULL = 0x10
    #: Write a newline at the end of the JSON string.
    WRITE_NEWLINE_AT_END = 0x80


def load(fp):
    return Document(fp.read()).as_obj


def loads(s):
    return Document(s).as_obj


def dumps(obj, *, default=None):
    return Document(obj, default=default).dumps()


def dump(obj, fp, *, default=None):
    fp.write(Document(obj, default=default).dumps())
