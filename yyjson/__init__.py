__all__ = [
    "Document",
    "ReaderFlags",
    "WriterFlags",
    "SAXHandler",
    "sax",
    "loads",
    "load",
    "dumps",
    "dump",
]

import enum
from typing import Any, Callable, Optional

from cyyjson import Document, sax, loads


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
    #: Alias for `NUMBERS_AS_DECIMAL`.
    NUMBERS_AS_RAW = 0x20
    #: Read all numbers as Decimal objects instead of native types. This option
    #: is useful for preserving the exact precision of numbers or for handling
    #: numbers that are too large to fit in a native type.
    NUMBERS_AS_DECIMAL = 0x20
    #: Alias for `BIG_NUMBERS_AS_DECIMAL`.
    BIGNUM_AS_RAW = 0x80
    #: Like `NUMBERS_AS_DECIMAL`, but only for numbers that are too large to
    #: fit in a native type.
    BIG_NUMBERS_AS_DECIMAL = 0x80


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


class SAXHandler:
    """
    Optional base class for :func:`sax` handlers.

    :func:`sax` accepts *any* object as a handler and calls each event method
    only if it exists, so subclassing this is never required - but doing so
    and overriding just the events you care about gives you IDE completion
    and static type checking for free:

    .. code-block:: python

        class KeyCollector(SAXHandler):
            def __init__(self):
                self.keys = []

            def key(self, value):
                self.keys.append(value)

    Unimplemented events are skipped entirely. A handler method may return
    ``False`` to stop parsing early.
    """

    #: Called when an object opens (``{``).
    obj_begin: Optional[Callable[[], Any]] = None
    #: Called when an object closes (``}``), with its member count.
    obj_end: Optional[Callable[[int], Any]] = None
    #: Called when an array opens (``[``).
    arr_begin: Optional[Callable[[], Any]] = None
    #: Called when an array closes (``]``), with its element count.
    arr_end: Optional[Callable[[int], Any]] = None
    #: Called for each object member key.
    key: Optional[Callable[[str], Any]] = None
    #: Called for each string value.
    string: Optional[Callable[[str], Any]] = None
    #: Called for each number value (``int``, ``float``, or ``Decimal``).
    number: Optional[Callable[[Any], Any]] = None
    #: Called for each boolean value.
    boolean: Optional[Callable[[bool], Any]] = None
    #: Called for each ``null``.
    null: Optional[Callable[[], Any]] = None


def load(fp):
    """
    Parse a JSON document from an open file-like object and return the
    equivalent Python object.

    The entire stream is read into memory before parsing. To process inputs
    too large for that, or to pull just a few values out of a huge document,
    see :func:`sax`.

    :param fp: An open file-like object with a ``read()`` method.
    :returns: The equivalent Python object.
    """
    return loads(fp.read())


def dumps(obj, *, default=None):
    """
    Serialize a Python object to a JSON ``str``.

    :param obj: The Python object to serialize.
    :param default: A function called to convert objects that are not
                    JSON serializable. Should return a JSON serializable
                    version of the object or raise a TypeError.
    :type default: callable, optional
    :returns: The serialized JSON document as a ``str``.
    """
    return Document.from_obj(obj, default=default).dumps()


def dump(obj, fp, *, default=None):
    """
    Serialize a Python object as JSON to an open file-like object.

    :param obj: The Python object to serialize.
    :param fp: An open file-like object with a ``write()`` method.
    :param default: A function called to convert objects that are not
                    JSON serializable. Should return a JSON serializable
                    version of the object or raise a TypeError.
    :type default: callable, optional
    """
    fp.write(Document.from_obj(obj, default=default).dumps())
