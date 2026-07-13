"""
Tests for the ReaderFlags/WriterFlags additions (yyjson 0.11/0.12 flags).
"""
import math
import re

import pytest

from yyjson import Document, ReaderFlags, WriterFlags, loads


def test_flag_values_match_yyjson_header():
    """The enum values must match the vendored yyjson.h constants."""
    header = open("yyjson/yyjson.h").read()

    def c_value(name):
        m = re.search(rf"{name}\s+= 1 << (\d+);", header)
        return 1 << int(m.group(1))

    assert ReaderFlags.ALLOW_BOM == c_value("YYJSON_READ_ALLOW_BOM")
    assert ReaderFlags.ALLOW_EXT_NUMBER == c_value("YYJSON_READ_ALLOW_EXT_NUMBER")
    assert ReaderFlags.ALLOW_EXT_ESCAPE == c_value("YYJSON_READ_ALLOW_EXT_ESCAPE")
    assert ReaderFlags.ALLOW_EXT_WHITESPACE == c_value(
        "YYJSON_READ_ALLOW_EXT_WHITESPACE"
    )
    assert ReaderFlags.ALLOW_SINGLE_QUOTED_STR == c_value(
        "YYJSON_READ_ALLOW_SINGLE_QUOTED_STR"
    )
    assert ReaderFlags.ALLOW_UNQUOTED_KEY == c_value("YYJSON_READ_ALLOW_UNQUOTED_KEY")
    assert WriterFlags.LOWERCASE_HEX == c_value("YYJSON_WRITE_LOWERCASE_HEX")
    assert WriterFlags.ALLOW_INVALID_UNICODE == c_value(
        "YYJSON_WRITE_ALLOW_INVALID_UNICODE"
    )
    assert WriterFlags.FP_TO_FLOAT == 1 << 27
    assert WriterFlags.fp_to_fixed(15) == 15 << 28
    # JSON5 mirrors the composite in yyjson.h
    assert ReaderFlags.JSON5 == (
        ReaderFlags.ALLOW_TRAILING_COMMAS
        | ReaderFlags.ALLOW_COMMENTS
        | ReaderFlags.ALLOW_INF_AND_NAN
        | ReaderFlags.ALLOW_EXT_NUMBER
        | ReaderFlags.ALLOW_EXT_ESCAPE
        | ReaderFlags.ALLOW_EXT_WHITESPACE
        | ReaderFlags.ALLOW_SINGLE_QUOTED_STR
        | ReaderFlags.ALLOW_UNQUOTED_KEY
    )


def test_allow_bom():
    data = b'\xef\xbb\xbf{"a": 1}'
    with pytest.raises(ValueError):
        Document(data)
    assert Document(data, flags=ReaderFlags.ALLOW_BOM).as_obj == {"a": 1}


def test_json5():
    doc = """
    {
        // a comment
        unquoted: 'single-quoted',
        hex: 0x7B,
        leadingDot: .5,
        nan: NaN,
        trailing: [1, 2, 3,],
    }
    """
    with pytest.raises(ValueError):
        Document(doc)
    obj = Document(doc, flags=ReaderFlags.JSON5).as_obj
    assert obj["unquoted"] == "single-quoted"
    assert obj["hex"] == 0x7B
    assert obj["leadingDot"] == 0.5
    assert math.isnan(obj["nan"])
    assert obj["trailing"] == [1, 2, 3]


def test_individual_json5_flags():
    assert Document(
        "{a: 1}", flags=ReaderFlags.ALLOW_UNQUOTED_KEY
    ).as_obj == {"a": 1}
    assert Document(
        "['x']", flags=ReaderFlags.ALLOW_SINGLE_QUOTED_STR
    ).as_obj == ["x"]
    assert Document("[0xFF]", flags=ReaderFlags.ALLOW_EXT_NUMBER).as_obj == [255]


def test_fp_to_float():
    third = 1.0 / 3.0
    assert Document({"v": third}).dumps() == '{"v":0.3333333333333333}'
    assert (
        Document({"v": third}).dumps(flags=WriterFlags.FP_TO_FLOAT)
        == '{"v":0.33333334}'
    )


def test_fp_to_fixed():
    third = 1.0 / 3.0
    assert (
        Document({"v": third}).dumps(flags=WriterFlags.fp_to_fixed(2))
        == '{"v":0.33}'
    )
    # trailing zeros are removed
    assert Document({"v": 0.5}).dumps(flags=WriterFlags.fp_to_fixed(6)) == '{"v":0.5}'
    # composes with other flags
    out = Document({"v": third}).dumps(
        flags=WriterFlags.fp_to_fixed(3) | WriterFlags.WRITE_NEWLINE_AT_END
    )
    assert out == '{"v":0.333}\n'
    with pytest.raises(ValueError):
        WriterFlags.fp_to_fixed(0)
    with pytest.raises(ValueError):
        WriterFlags.fp_to_fixed(16)


def test_lowercase_hex():
    doc = Document({"s": "é"})
    upper = doc.dumps(flags=WriterFlags.ESCAPE_UNICODE)
    lower = doc.dumps(flags=WriterFlags.ESCAPE_UNICODE | WriterFlags.LOWERCASE_HEX)
    assert upper == '{"s":"\\u00E9"}'
    assert lower == '{"s":"\\u00e9"}'
