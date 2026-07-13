"""
Path handling must work with non-ASCII names on every platform. On Windows in
particular, narrow fopen() interprets paths in the ANSI codepage, so these
only pass if the extension routes paths through the wide-char API.
"""
import pytest

from yyjson import Document, sax, loads

NON_ASCII_NAMES = [
    "café.json",          # latin-1 range
    "日本語.json",         # BMP, outside latin-1
    "emoji-😀.json",      # astral plane
    "mixed-šžé-中.json",  # a bit of everything
]


def non_ascii_file(tmp_path, name, payload=b'{"hello": "world"}'):
    """Create a file with a non-ASCII name, skipping if the filesystem can't
    represent it (e.g. a non-UTF-8 POSIX locale)."""
    path = tmp_path / name
    try:
        path.write_bytes(payload)
    except (OSError, UnicodeEncodeError):
        pytest.skip(f"filesystem cannot represent {name!r}")
    return path


@pytest.mark.parametrize("name", NON_ASCII_NAMES)
def test_document_non_ascii_path(tmp_path, name):
    path = non_ascii_file(tmp_path, name)
    assert Document(path).as_obj == {"hello": "world"}


@pytest.mark.parametrize("name", NON_ASCII_NAMES)
def test_loads_non_ascii_path(tmp_path, name):
    path = non_ascii_file(tmp_path, name)
    assert loads(path) == {"hello": "world"}


@pytest.mark.parametrize("name", NON_ASCII_NAMES)
def test_sax_non_ascii_path(tmp_path, name):
    class KeyCollector:
        def __init__(self):
            self.keys = []

        def key(self, value):
            self.keys.append(value)

    path = non_ascii_file(tmp_path, name)
    collector = KeyCollector()
    sax(path, collector)
    assert collector.keys == ["hello"]


def test_document_non_ascii_dir(tmp_path):
    """Non-ASCII directory components, not just the file name."""
    subdir = tmp_path / "ディレクトリ"
    try:
        subdir.mkdir()
    except (OSError, UnicodeEncodeError):
        pytest.skip("filesystem cannot represent non-ASCII directory")
    path = non_ascii_file(subdir, "fichier-à.json")
    assert Document(path).as_obj == {"hello": "world"}


def test_missing_path_raises_oserror(tmp_path):
    """Opening a missing file reports the OS error with the filename."""
    path = tmp_path / "does-not-exist-é.json"
    with pytest.raises(OSError) as excinfo:
        Document(path)
    assert excinfo.value.filename == path
    with pytest.raises(OSError):
        sax(path, object())
