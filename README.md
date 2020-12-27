![PyPI - License](https://img.shields.io/pypi/l/yyjson.svg?style=flat-square)
![Tests](https://github.com/TkTech/py_yyjson/workflows/Run%20tests/badge.svg)

# py_yyjson

Exploratory python bindings for the [yyjson][] project.

This is a work in progress, and while technically it *works*, it needs to be
fleshed out, documentation needs to be added, tests, CI, etc...

Find the latest documentation at https://py_yyjson.tkte.ch.

[yyjson]: https://github.com/ibireme/yyjson

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

py_yyjson compares well against most libraries. The [full benchmarks][bench]
can be found in its sister project, [pysimdjson][].


[bench]: https://github.com/TkTech/pysimdjson#-benchmarks
[pysimdjson]: https://github.com/TkTech/pysimdjson
