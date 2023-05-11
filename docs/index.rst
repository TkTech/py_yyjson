[py]yyjson
==========

.. toctree::
  :hidden:

  api.rst
  development.rst

Python bindings to the fantastic `yyjson`_ project. This module provides a fast,
flexible, portable, and correct JSON parser and serializer.

Binary packages are provided for many versions of Python on many architectures,
and only requires a C89-compliant compiler when building from source.

Sales Pitch
-----------

[py]yyjson is several times faster than the builtin JSON module, and is faster
than most other JSON libraries. It's also more flexible, allowing you to parse
JSON with strict specification compliance, or with extensions such as comments,
trailing commas, Inf/NaN, and more.

For all Python JSON libraries, the majority of time isn't spent parsing the
JSON, it's spent creating Python objects to represent that JSON. [py]yyjson
can provide significant speedups by avoiding creating Python objects for the
entire document, allowing you to extract just the parts of the document you
actually care about. It also provides facilities for manipulating the
document in native code, such as performing a JSON Merge-Patch (RFC 7386)
or a JSON Patch (RFC 6902), avoiding creating Python objects entirely.

[py]yyjson is a lightweight project dependency with low maintenance overhead.
It's written in C, and has no dependencies other than a C89 compiler. It's
licensed under the MIT license, so you can use it in any project, even
commercial ones. Pre-built binary wheels are available for many versions of
Python on many architectures, such as x86, x86_64, ARM, and ARM64, PowerPC,
IBM Z, and more. PyPy is also supported.


Installation
------------

If binary wheels are available for your platform, you can install the latest
version of [py]yyjson with pip:

    pip install yyjson

If you want to build from source, or if binary wheels aren't available, you'll
need a C89 compiler, such as GCC or Clang.


Benchmarks
----------

You can find the benchmarks for this project in its sister project,
`json_benchmark`_. yyjson compares favourably, and often beats, most other
libraries.

Examples
--------

Load a document
^^^^^^^^^^^^^^^

Simply parse an entire JSON document to a Python object::

    from pathlib import Path
    from yyjson import Document

    doc = Document(Path("canada.json")).as_obj


Load part of a document
^^^^^^^^^^^^^^^^^^^^^^^

Parse a JSON document, but only extract the part you care about by using
a JSON Pointer::

    from pathlib import Path
    from yyjson import Document

    doc = Document(Path("canada.json"))
    features = doc.get_pointer("/features")


Patch a document
^^^^^^^^^^^^^^^^

Add an entry to a GeoJSON file without deserializing the entire document
into Python objects::

    from pathlib import Path
    from yyjson import Document

    doc = Document(Path("canada.json")
    patch = Document([
        {'op': 'add', 'path': '/features/-', 'value': {
            'type': 'Feature',
            'geometry': {
                'type': 'Point',
                'coordinates': [1, 2],
            },
            'properties': {
                'name': 'New Feature',
            },
        }},
    ])
    modified = doc.patch(patch)
    print(modified.dumps())


Serialize an object
^^^^^^^^^^^^^^^^^^^

Serialize a Python object to JSON::

    from yyjson import Document

    doc = Document({
      "hello": "world"
    })
    print(doc.dumps())

.. _yyjson: https://github.com/ibireme/yyjson
.. _json_benchmark: https://github.com/tktech/json_benchmark