![PyPI - License](https://img.shields.io/pypi/l/yyjson.svg?style=flat-square)
![Tests](https://github.com/TkTech/py_yyjson/workflows/Run%20tests/badge.svg)

# py_yyjson

Python bindings for the excellent [yyjson][] project.

## Documentation

Find the latest documentation at https://py_yyjson.tkte.ch.

[yyjson]: https://github.com/ibireme/yyjson

## 💡 Why should I use this?

Like everything, it has pros and cons versus other Python JSON parsers. Some of
the pros:

- Good configurable support for **non-standard JSON**, such as comments, NaN,
  Infinity, and numbers of any size.
- **Fast**. yyjson has excellent performance, especially compared to Python's
  built-in JSON library.
- Support for JSON Merge-Patch.
 
For more technical users, some additional pros:

- Building from source is architecture independent (ex, no dependency on SIMD 
  instructions) and requires just a C89 compiler.
- Uses the Python memory allocator, so Python memory profiling tool will work
  as expected.

## 🎉 Installation

If binary wheels are available for your platform, you can install from pip
with no further requirements:

    pip install yyjson

Binary wheels are available for the following:

|                  | py3.5 | py3.6 | py3.7 | py3.8 | py3.9 | pypy3 |
| ---------------- | ----- | ----- | ----- | ----- | ----- | ----- |
| OS X (x86_64)    | y     | y     | y     | y     | y     | y     |
| Windows (x86_64) | y     | y     | y     | y     | y     | y     |
| Linux (x86_64)   | y     | y     | y     | y     | y     | y     |

If binary wheels are not available for your platform, you'll need any
C89-compatible compiler.

    pip install 'yyjson' --no-binary :all:

## ⚗ Development and Testing

To install test requirements, use:

    pip install -e ".[test]"

To run the tests, just type `pytest`. To prepare for a release or to rebuild
documentation, you need a few extra dependencies:

    pip install -e ".[release]"

You can then rebuild the documentation by running `make html` within the
`docs/` directory.

## 📈 Benchmarks

py_yyjson compares well against most libraries. The full benchmarks can be
found in its sister project [json_benchmark][].


[pysimdjson]: https://github.com/TkTech/pysimdjson
[json_benchmark]: https://github.com/tktech/json_benchmark
