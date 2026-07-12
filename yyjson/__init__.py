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
    #: Allow a UTF-8 BOM at the start of the input, as commonly produced by
    #: Windows tooling.
    ALLOW_BOM = 0x100
    #: Allow extended number formats, such as hex (``0x7B``), a leading dot
    #: (``.123``), or a leading plus sign.
    ALLOW_EXT_NUMBER = 0x200
    #: Allow extended escape sequences, such as ``\\a``, ``\\0``, ``\\x7B``.
    ALLOW_EXT_ESCAPE = 0x400
    #: Allow extended whitespace, such as ``\\v``, ``\\f``, or U+2028.
    ALLOW_EXT_WHITESPACE = 0x800
    #: Allow single-quoted strings, such as ``'hello'``.
    ALLOW_SINGLE_QUOTED_STR = 0x1000
    #: Allow unquoted object keys, such as ``{a: 1}``.
    ALLOW_UNQUOTED_KEY = 0x2000
    #: Parse `JSON5 <https://json5.org>`_: comments, trailing commas,
    #: Inf/NaN, extended numbers, escapes and whitespace, single-quoted
    #: strings, and unquoted keys. Combine with ``ALLOW_BOM`` if the input
    #: may start with a BOM. Only the DOM readers (``Document``, ``loads``)
    #: support JSON5; ``sax()`` ignores non-standard flags.
    JSON5 = (
        0x04  # ALLOW_TRAILING_COMMAS
        | 0x08  # ALLOW_COMMENTS
        | 0x10  # ALLOW_INF_AND_NAN
        | 0x200  # ALLOW_EXT_NUMBER
        | 0x400  # ALLOW_EXT_ESCAPE
        | 0x800  # ALLOW_EXT_WHITESPACE
        | 0x1000  # ALLOW_SINGLE_QUOTED_STR
        | 0x2000  # ALLOW_UNQUOTED_KEY
    )


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
    #: Write strings containing invalid unicode as-is instead of raising an
    #: error. The output may not be valid JSON or UTF-8.
    ALLOW_INVALID_UNICODE = 0x20
    #: Write a newline at the end of the JSON string.
    WRITE_NEWLINE_AT_END = 0x80
    #: Write ``\\uXXXX`` escapes with lowercase hex digits.
    LOWERCASE_HEX = 0x100
    #: Write floating-point numbers as single-precision (``double`` is cast
    #: to ``float`` first): shorter output, may lose precision. Ignored when
    #: combined with :meth:`fp_to_fixed`.
    FP_TO_FLOAT = 0x08000000

    @staticmethod
    def fp_to_fixed(precision):
        """
        A flag to write floating-point numbers using fixed-point notation
        with the given number of decimals (1-15), like
        ``Number.prototype.toFixed(precision)`` but with trailing zeros
        removed. Combine it with other flags, e.g.
        ``dumps(flags=WriterFlags.PRETTY | WriterFlags.fp_to_fixed(3))``.

        :param precision: Number of decimal places, 1 through 15.
        :returns: The flag value as an ``int``.
        """
        if not 1 <= precision <= 15:
            raise ValueError("precision must be between 1 and 15")
        return precision << 28


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
