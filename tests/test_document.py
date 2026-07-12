import math

import pytest

from yyjson import Document, WriterFlags, ReaderFlags

# The maximum value of a signed 64 bit value.
LLONG_MAX = 9223372036854775807
# The maximum value of an unsigned 64 bit value.
ULLONG_MAX = 18446744073709551615


def test_document_is_mutable():
    """Ensure we can determine if a document is mutable."""
    doc = Document({"hello": "world"})
    assert doc.is_thawed is True

    doc = Document('{"hello": "world"}')
    assert doc.is_thawed is False


def test_document_from_str():
    """Ensure we can parse a document from a str."""
    doc = Document('{"hello": "world"}')
    assert doc.as_obj == {"hello": "world"}


def test_document_from_json():
    """Document.from_json always parses str/bytes/Path as JSON text."""
    doc = Document.from_json('{"a": 1}')
    assert doc.as_obj == {"a": 1}
    assert doc.is_thawed is False  # parsed documents are immutable

    assert Document.from_json(b'{"a": 1}').as_obj == {"a": 1}

    # Reader flags are honored.
    doc = Document.from_json(
        '{"a": 1,}', flags=ReaderFlags.ALLOW_TRAILING_COMMAS
    )
    assert doc.as_obj == {"a": 1}

    # A non-JSON-text argument is a TypeError, not a silent build-from-object.
    with pytest.raises(TypeError):
        Document.from_json({"a": 1})
    with pytest.raises(TypeError):
        Document.from_json(123)

    # Invalid JSON raises ValueError.
    with pytest.raises(ValueError):
        Document.from_json("not json")


def test_document_from_json_path(tmp_path):
    """Document.from_json reads and parses a file given a Path."""
    p = tmp_path / "doc.json"
    p.write_text('{"greeting": "café 日本"}', encoding="utf-8")
    assert Document.from_json(p).as_obj == {"greeting": "café 日本"}


def test_document_types():
    """Ensure each primitive type can be upcast (which does not have its own
    dedicated test.)"""
    values = (
        ('"hello"', "hello"),
        ("1", 1),
        ("-1", -1),
        ('{"hello": "world"}', {"hello": "world"}),
        ("[0, 1, 2]", [0, 1, 2]),
    )

    for src, dst in values:
        doc = Document(src)
        assert doc.as_obj == dst


def test_document_unicode_as_obj():
    """Non-ASCII strings must survive the round-trip through .as_obj, not just
    dumps(). Regression test for the ASCII fast-path miscounting multi-byte
    UTF-8 continuation bytes."""
    cases = [
        "café",                     # 2-byte sequences
        "naïve",
        "日本語",                    # 3-byte sequences
        "Ω≈ç√",
        "🙇🎉",                     # 4-byte sequences (astral plane)
        "mixed café 日本 🙇 tail",  # ASCII interleaved with multi-byte
    ]
    for value in cases:
        doc = Document('{"key": "%s"}' % value)
        assert doc.as_obj == {"key": value}

    # Non-ASCII object keys go through the same fast-path.
    doc = Document('{"café": "value"}')
    assert doc.as_obj == {"café": "value"}

    # The bytes input path decodes through the same conversion.
    doc = Document('["日本語"]'.encode("utf-8"))
    assert doc.as_obj == ["日本語"]


def test_document_dumps():
    """
    Ensure we can properly dump a document to a string.
    """
    doc = Document('{"hello": "world"}')

    # Minified by default.
    assert doc.dumps() == '{"hello":"world"}'
    assert doc.dumps(flags=WriterFlags.PRETTY) == ("{\n" '    "hello": "world"\n' "}")

    doc = Document("{}")
    assert doc.dumps() == "{}"

    doc = Document({})
    assert doc.dumps() == "{}"

    doc = Document([])
    assert doc.dumps() == "[]"

    doc = Document({"hello": {"there": [0, 1, 2]}})
    assert doc.dumps(at_pointer="/hello/there") == "[0,1,2]"


def test_document_dumps_nan_and_inf():
    """
    Ensure we can dump documents with Infinity and NaN.
    """
    # In standards mode, NaN & Inf should be a hard error.
    with pytest.raises(ValueError):
        Document('{"hello": NaN}')

    with pytest.raises(ValueError):
        Document('{"hello": Infinity}')

    doc = Document(
        """{
        "hello": NaN,
        "world": Infinity
    }""",
        flags=ReaderFlags.ALLOW_INF_AND_NAN,
    )
    obj = doc.as_obj
    assert math.isnan(obj["hello"])
    assert math.isinf(obj["world"])


def test_document_raw_type():
    """
    Ensure we can dump objects that contain integers of any size. This is
    against the JSON specification, but the same as the built-in JSON module.

    In YYJSON, these values are stored in yyjson_raw, which essentially just
    points to the value as a string and does not attempt to interpret it as a
    number.
    """
    # Ensure the maximum yyjson_sint value can be stored.
    doc = Document([LLONG_MAX])
    assert doc.dumps() == "[9223372036854775807]"

    # Ensure the maximum yyjson_sint value + 1 can be stored as a yyjson_uint.
    doc = Document([LLONG_MAX + 1])
    assert doc.dumps() == "[9223372036854775808]"

    # Ensure the maximum yyjson_uint value can be stored.
    doc = Document([ULLONG_MAX])
    assert doc.dumps() == "[18446744073709551615]"

    # Ensure the maximum yyjson_uint value + 1 can be stored as a yyjson_raw.
    doc = Document([ULLONG_MAX + 1])
    assert doc.dumps() == "[18446744073709551616]"
    assert doc.as_obj == [ULLONG_MAX + 1]

    # Ensure we can parse a document with and without the RAW flags set.
    doc = Document("[18446744073709551616000000000]", flags=ReaderFlags.NUMBERS_AS_RAW)
    assert doc.dumps() == "[18446744073709551616000000000]"
    assert doc.as_obj == [18446744073709551616000000000]


def test_document_float_type():
    """
    Ensure we can load and dump floats.
    """
    doc = Document([1.25])
    assert doc.dumps() == "[1.25]"
    assert doc.as_obj == [1.25]

    doc = Document("1.25")
    assert doc.dumps() == "1.25"
    assert doc.as_obj == 1.25


def test_document_boolean_type():
    """
    Ensure we can load and dump boolean types.
    """
    doc = Document("true")
    assert doc.dumps() == "true"
    assert doc.as_obj is True

    doc = Document("false")
    assert doc.dumps() == "false"
    assert doc.as_obj is False

    doc = Document([True])
    assert doc.dumps() == "[true]"
    assert doc.as_obj == [True]

    doc = Document([False])
    assert doc.dumps() == "[false]"
    assert doc.as_obj == [False]

def test_document_list_type():
    doc = Document('[1,2,3,4]')
    assert doc.dumps() == '[1,2,3,4]'
    assert doc.as_obj == [1, 2, 3, 4]

    doc = Document([1, 2, 3, 4])
    assert doc.dumps() == '[1,2,3,4]'
    assert doc.as_obj == [1, 2, 3, 4]

def test_document_tuple_type():
    doc = Document(())
    assert doc.dumps() == '[]'

    doc = Document((1,))
    assert doc.dumps() == '[1]'

    doc = Document((1, 2, 3, 4))
    assert doc.dumps() == '[1,2,3,4]'

    doc = Document([(1, 2), (3, 4)])
    assert doc.dumps() == '[[1,2],[3,4]]'

    doc = Document({'test': (1, 2)})
    assert doc.dumps() == '{"test":[1,2]}'

def test_document_none_type():
    """
    Ensure we can load and dump the None type.
    """
    doc = Document("null")
    assert doc.dumps() == "null"
    assert doc.as_obj is None

    doc = Document([None])
    assert doc.dumps() == "[null]"
    assert doc.as_obj == [None]


def test_document_dict_type():
    """
    Ensure we can load and dump the dict type.
    """
    doc = Document('{"a": "b"}')
    assert doc.dumps() == '{"a":"b"}'
    assert doc.as_obj == {'a': 'b'}

    doc = Document({"a": "b"})
    assert doc.dumps() == '{"a":"b"}'
    assert doc.as_obj == {'a': 'b'}

    with pytest.raises(TypeError) as exc:
        Document({1: 'b'})
    assert exc.value.args[0] == 'Dictionary keys must be strings'

    with pytest.raises(TypeError) as exc:
        Document({'\ud83d\ude47': 'foo'})
    assert exc.value.args[0] == 'Dictionary keys must be strings'


def test_document_get_pointer():
    """
    Ensure JSON pointers work.
    """
    doc = Document(
        """{
        "size" : 3,
        "users" : [
            {"id": 1, "name": "Harry"},
            {"id": 2, "name": "Ron"},
            {"id": 3, "name": "Hermione"}
        ]}
    """
    )

    assert doc.get_pointer("/size") == 3
    assert doc.get_pointer("/users/0") == {"id": 1, "name": "Harry"}
    assert doc.get_pointer("/users/1/name") == "Ron"

    with pytest.raises(ValueError) as exc:
        doc.get_pointer("bob")

    assert "no prefix" in str(exc.value)

    doc = Document(
        {
            "size": 3,
            "users": [
                {"id": 1, "name": "Harry"},
                {"id": 2, "name": "Ron"},
                {"id": 3, "name": "Hermione"},
            ],
        }
    )

    assert doc.get_pointer("/size") == 3
    assert doc.get_pointer("/users/0") == {"id": 1, "name": "Harry"}
    assert doc.get_pointer("/users/1/name") == "Ron"

    with pytest.raises(ValueError):
        doc.get_pointer("bob")


def test_document_length():
    """
    Ensure we can get the length of mapping types.
    """
    doc = Document("""{"hello": "world"}""")
    assert len(doc) == 1

    doc = Document("""[0, 1, 2]""")
    assert len(doc) == 3

    doc = Document("1")
    assert len(doc) == 0

    doc = Document({})
    assert len(doc) == 0

    doc = Document([0, 1, 2])
    assert len(doc) == 3


def test_document_freeze():
    """
    Ensure we can freeze mutable documents.
    """
    # Documents created from Python objects are always mutable by default,
    # so use that as our starting point.
    doc = Document({"hello": "world"})
    assert doc.is_thawed is True

    doc.freeze()
    assert doc.is_thawed is False


def test_document_size():
    """
    Test the size attribute that returns the size of data read from original JSON input.
    """
    # Test with immutable document (created from JSON string)
    json_str = '{"hello": "world", "number": 42}'
    doc = Document(json_str)
    assert doc.bytes_read == len(json_str)

    # Test with different sized JSON inputs
    small_json = "{}"
    doc_small = Document(small_json)
    assert doc_small.bytes_read == len(small_json)

    large_json = '{"users": [{"id": 1, "name": "Alice"}, {"id": 2, "name": "Bob"}], "count": 2}'
    doc_large = Document(large_json)
    assert doc_large.bytes_read == len(large_json)

    # Test with mutable document (created from Python object) - should return 0
    doc_mutable = Document({"hello": "world"})
    assert doc_mutable.bytes_read == 0


def test_document_deeply_nested_raises():
    """Deeply nested input must raise a catchable RecursionError rather than
    overflowing the C stack, matching the stdlib json module."""
    depth = 100_000

    # Parsing succeeds (yyjson's reader is iterative); materializing to Python
    # objects recurses and must raise instead of segfaulting.
    doc = Document("[" * depth + "]" * depth)
    with pytest.raises(RecursionError):
        doc.as_obj

    # Serializing a deeply nested Python object recurses on the way in.
    nested = []
    current = nested
    for _ in range(depth):
        child = []
        current.append(child)
        current = child
    with pytest.raises(RecursionError):
        Document(nested)


class _NonBlockingReadinto:
    """Simulates a non-blocking binary stream: chunks, then None forever."""

    def __init__(self, *chunks):
        self._chunks = list(chunks)

    def readinto(self, buf):
        if not self._chunks:
            return None
        chunk = self._chunks.pop(0)
        buf[: len(chunk)] = chunk
        return len(chunk)


class _NonBlockingRead:
    def __init__(self, *chunks):
        self._chunks = list(chunks)

    def read(self, n):
        if not self._chunks:
            return None
        return self._chunks.pop(0)


def test_stream_nonblocking_readinto_raises():
    """None from readinto() means "no data yet", not EOF. Previously this
    silently truncated: b'[1]' followed by None parsed as [1]."""
    with pytest.raises(BlockingIOError):
        Document(_NonBlockingReadinto(b"[1]"))
    with pytest.raises(BlockingIOError):
        Document(_NonBlockingReadinto())


def test_stream_nonblocking_read_raises():
    with pytest.raises(BlockingIOError):
        Document(_NonBlockingRead(b'{"a": 1}'))
    with pytest.raises(BlockingIOError):
        Document(_NonBlockingRead())


def test_thawed_conversion_parity():
    """The thawed (mutable) conversion path shares one implementation with
    the frozen path; all scalar types and containers round-trip."""
    from decimal import Decimal

    obj = {
        "nums": [1, -2, 3.5, 2**80, Decimal("1.23")],
        "nested": {"a": [True, False, None, "x"]},
        "unicode": "café € 日本語",
        "empty": {},
        "repeated_keys": [{"id": i, "name": "n"} for i in range(100)],
    }
    doc = Document(obj)
    assert doc.is_thawed
    assert doc.as_obj == obj


def test_thawed_deep_nesting_raises():
    """Conversion depth is capped at 1024 for thawed documents too (it always
    was for frozen ones)."""
    deep = cur = []
    for _ in range(1100):
        nxt = []
        cur.append(nxt)
        cur = nxt
    doc = Document(deep)
    with pytest.raises(RecursionError):
        doc.as_obj
