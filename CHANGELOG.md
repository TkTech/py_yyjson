# Changelog

## v5.0.0 (unreleased)

### Added

- A SAX-style streaming parser, `yyjson.sax()`. It parses inputs of unlimited
  size with bounded memory - a fixed sliding window plus the nesting depth -
  calling optional methods on a handler object for each token, and can stop
  early when a handler returns `False`. Sources can be `str`, `bytes`, a
  `pathlib.Path`, or a binary file-like object.
- `Document()` accepts binary file-like objects and parses them directly into
  a document.
- The module-level `loads()` is now implemented in C and accepts `str`,
  `bytes`, `bytearray`, or `pathlib.Path`.
- `Document.from_obj()` and `Document.from_json()` classmethods - explicit
  counterparts to the type-guessing constructor, resolving the `Document(str)`
  ambiguity between "parse this JSON text" and "build from this Python
  string".
- `Document.patch()` accepts any `Document`-constructible value (a `dict`,
  `list`, JSON text, or `Path`), wrapping it in a temporary `Document`.
- `Document.bytes_read`: the number of input bytes consumed by the parse
  (useful with `ReaderFlags.STOP_WHEN_DONE`, e.g. for NDJSON).
- Type annotations: a `py.typed` marker and `.pyi` stubs for the C extension.
- Python 3.14 and PyPy 3.11 wheels.

### Changed

- Major performance work on the read path - object-key caching, direct
  document-tape walking, an ASCII fast path, presized containers, and pausing
  the cyclic GC during conversion. `loads()` and `Document.as_obj` now beat or
  match the fastest Python JSON libraries on most benchmarks.
- Parsing from file-like objects and `Path`s now reads into a single owned
  buffer parsed in place: roughly 20% faster with roughly 20% lower peak
  memory.
- Dictionary keys must be strings when serializing; other key types now raise
  `TypeError` instead of producing undefined behaviour.
- Tuples are serialized as JSON arrays.
- Converting documents nested deeper than 1024 container levels raises
  `RecursionError` instead of risking a C stack overflow.
- `flags` and `default` arguments are keyword-only. (They always were at
  runtime; the documentation and type stubs now agree.)
- Wheels are no longer built against the limited API (required by the new
  ASCII fast path), and EOL PyPy 3.9/3.10 wheels are no longer produced.
- The vendored yyjson was upgraded to 0.12.0.

### Fixed

- A UTF-8 corruption bug on platforms where `char` is signed.
- A possible segfault when parsing extremely deep documents.
- A crash when `Document.patch()` was called with a non-`Document` value.
- A memory leak when serializing integers larger than 64 bits.
- A memory leak when `Document.__init__()` was invoked more than once on the
  same object.
