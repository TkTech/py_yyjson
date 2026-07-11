"""
Tests for the streaming (SAX) reader, ``yyjson.sax``.

The central test reconstructs a Python object from the SAX event stream and
asserts it equals the DOM parse (``Document.as_obj``) -- across many window
sizes and, for file-like sources, tiny read chunks that force the C window to
compact and rebase at every byte boundary.
"""

import io
import json
import decimal

import pytest

import yyjson
from yyjson import ReaderFlags


class Builder:
    """A handler that rebuilds the native Python object from SAX events."""

    def __init__(self):
        self._stack = []          # containers being built
        self._keys = []           # pending key per open object
        self.result = None

    def _emit(self, value):
        if not self._stack:
            self.result = value
            return
        top = self._stack[-1]
        if isinstance(top, dict):
            top[self._keys[-1]] = value
        else:
            top.append(value)

    def obj_begin(self):
        d = {}
        self._emit(d)
        self._stack.append(d)
        self._keys.append(None)

    def obj_end(self, count):
        assert len(self._stack.pop()) == count
        self._keys.pop()

    def arr_begin(self):
        a = []
        self._emit(a)
        self._stack.append(a)

    def arr_end(self, count):
        assert len(self._stack.pop()) == count

    def key(self, value):
        self._keys[-1] = value

    def string(self, value):
        self._emit(value)

    def number(self, value):
        self._emit(value)

    def boolean(self, value):
        self._emit(value)

    def null(self):
        self._emit(None)


class ChunkedReader:
    """A binary file-like that yields at most ``chunk`` bytes per readinto."""

    def __init__(self, data, chunk):
        self.data = data
        self.pos = 0
        self.chunk = chunk

    def readinto(self, buf):
        n = min(len(buf), self.chunk, len(self.data) - self.pos)
        buf[:n] = self.data[self.pos:self.pos + n]
        self.pos += n
        return n


def sax_to_obj(source, **kw):
    b = Builder()
    yyjson.sax(source, b, **kw)
    return b.result


DOCUMENTS = [
    "null", "true", "false", "42", "-17", "3.14", "1e10", '""', '"hello"',
    '"a\\"b\\\\c\\n\\u00e9"', "[]", "{}", "[1,2,3]", '{"a":1,"b":2}',
    '{"nested":{"a":[1,2,{"b":null}],"c":"x"},"list":[[],[1],[[2]]]}',
    '[null,true,false,"s",1,2.5,-3,"\\u20ac"]',
    '{"unicode":"café €","empty":"","deep":[[[[[1]]]]]}',
]


@pytest.mark.parametrize("doc", DOCUMENTS)
def test_sax_matches_dom_bytes(doc):
    expected = yyjson.Document(doc).as_obj
    for window in (0, 8192, 65536):
        got = sax_to_obj(doc.encode("utf-8"), window_size=window)
        assert got == expected, (doc, window)


@pytest.mark.parametrize("doc", DOCUMENTS)
def test_sax_matches_dom_str(doc):
    assert sax_to_obj(doc) == yyjson.Document(doc).as_obj


@pytest.mark.parametrize("doc", DOCUMENTS)
def test_sax_filelike_tiny_chunks(doc):
    """Force compaction/rebasing at every byte boundary with 1..3 byte reads."""
    data = doc.encode("utf-8")
    expected = yyjson.Document(doc).as_obj
    for chunk in (1, 2, 3, 7):
        got = sax_to_obj(ChunkedReader(data, chunk), window_size=8192)
        assert got == expected, (doc, chunk)


def test_sax_large_document_compaction():
    """A document much larger than the window streams correctly."""
    obj = [{"i": i, "name": f"item-{i}", "vals": [i, i * 2, i * 3]}
           for i in range(5000)]
    data = json.dumps(obj).encode("utf-8")
    assert len(data) > 8192  # ensures the window compacts many times
    got = sax_to_obj(ChunkedReader(data, 64), window_size=8192)
    assert got == obj


def test_sax_counts():
    counts = []

    class H:
        def obj_end(self, n):
            counts.append(("obj", n))

        def arr_end(self, n):
            counts.append(("arr", n))

    yyjson.sax(b'{"a":1,"b":2,"c":[1,2,3,4]}', H())
    assert counts == [("arr", 4), ("obj", 3)]


def test_sax_decimal_mode():
    got = sax_to_obj(b"[1, 2.5, 123456789012345678901234567890]",
                     flags=ReaderFlags.NUMBERS_AS_DECIMAL)
    assert got == [decimal.Decimal("1"), decimal.Decimal("2.5"),
                   decimal.Decimal("123456789012345678901234567890")]
    assert all(isinstance(x, decimal.Decimal) for x in got)


def test_sax_bignum_as_raw():
    got = sax_to_obj(b"[1, 2.5, 123456789012345678901234567890]",
                     flags=ReaderFlags.BIG_NUMBERS_AS_DECIMAL)
    assert got[0] == 1 and isinstance(got[0], int)
    assert got[1] == 2.5 and isinstance(got[1], float)
    assert got[2] == decimal.Decimal("123456789012345678901234567890")


def test_sax_early_stop_returns_false():
    seen = []

    class H:
        def number(self, v):
            seen.append(v)
            return v < 3   # stop once we hit 3

    # returns normally (no exception) even though parsing stopped early
    yyjson.sax(b"[1,2,3,4,5]", H())
    assert seen == [1, 2, 3]


def test_sax_exception_propagates():
    class Boom(Exception):
        pass

    class H:
        def string(self, v):
            raise Boom(v)

    with pytest.raises(Boom):
        yyjson.sax(b'["x"]', H())


def test_sax_missing_methods_are_skipped():
    # A handler with only one method still parses the whole document.
    strings = []

    class H:
        def string(self, v):
            strings.append(v)

    yyjson.sax(b'{"a":"x","b":[1,"y",{"c":"z"}]}', H())
    assert strings == ["x", "y", "z"]


@pytest.mark.parametrize("bad,exc", [
    (b"", ValueError),
    (b"[1,2", ValueError),
    (b'{"a":}', ValueError),
    (b"[1] trailing", ValueError),
])
def test_sax_errors(bad, exc):
    with pytest.raises(exc):
        yyjson.sax(bad, object())


def test_sax_stop_when_done_allows_trailing():
    # Only the first document is consumed; trailing content is not an error.
    got = sax_to_obj(b'{"a":1} {"b":2}', flags=ReaderFlags.STOP_WHEN_DONE)
    assert got == {"a": 1}


def test_sax_token_larger_than_window():
    big = b'"' + b"x" * 20000 + b'"'
    with pytest.raises(ValueError, match="window"):
        yyjson.sax(big, object(), window_size=8192)
    # fits with a larger window
    assert sax_to_obj(big, window_size=65536) == "x" * 20000


def test_sax_max_depth():
    data = b"[" * 100 + b"]" * 100
    with pytest.raises(ValueError, match="depth"):
        yyjson.sax(data, object(), max_depth=10)


def test_sax_bad_source_type():
    with pytest.raises(TypeError):
        yyjson.sax(1234, object())


def test_sax_path(tmp_path):
    p = tmp_path / "doc.json"
    p.write_bytes(b'{"from":"path","nums":[1,2,3]}')
    assert sax_to_obj(p) == {"from": "path", "nums": [1, 2, 3]}
