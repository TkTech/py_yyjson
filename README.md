![PyPI - License](https://img.shields.io/pypi/l/yyjson.svg?style=flat-square)
![Tests](https://github.com/TkTech/py_yyjson/workflows/Run%20tests/badge.svg)

# py_yyjson

![py_yyjson Logo](misc/logo_small.png)

Fast and flexible Python JSON parsing built on the excellent [yyjson][]
project.

![GitHub Sponsors](https://img.shields.io/github/sponsors/tktech)
![PyPI - License](https://img.shields.io/pypi/l/yyjson)
![PyPI - Version](https://img.shields.io/pypi/v/yyjson)

- **Fast**: `yyjson` is several times faster than the builtin JSON module, and
  is [faster than most other JSON libraries][fast].
- **Flexible**: Parse JSON with strict specification compliance, or with
  extensions such as comments, trailing commas, Inf/NaN, numbers of any size,
  and more.
- **Lightweight**: `yyjson` is a lightweight project dependency with low
  maintenance overhead. It's written in C, and has no dependencies other than
  a C89 compiler. Built wheels are between 50kb and 800kb depending on the
  platform.
- **Portable**: Binary wheels are available for many versions of Python
  on many architectures, such as x86, x86_64, ARM, and ARM64, PowerPC, IBM Z,
  and more. PyPy is also supported. Supports Python 3.9 and newer.
- **Manipulate documents**: The fastest JSON Merge-Patch (RFC 7386), JSON Patch
  (RFC 6902), and JSON Pointer (RFC 6901) implementations available for Python
  allow you to manipulate JSON documents without deserializing them into Python
  objects.
- **Traceable**: `yyjson` uses Python's memory allocator by default, so you can
  trace memory leaks and other memory issues using Python's built-in tools.

## Documentation

Find the latest documentation at https://tkte.ch/py_yyjson.

[yyjson]: https://github.com/ibireme/yyjson
[fast]: https://github.com/tktech/json_benchmark