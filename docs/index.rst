[py]yyjson
==========

.. toctree::
  :hidden:

  api.rst
  development.rst
  changelog.md

Fast and flexible Python JSON parsing built on the excellent `yyjson`_ library.

.. image:: https://img.shields.io/github/sponsors/tktech
   :alt: GitHub Sponsors

.. image:: https://img.shields.io/pypi/l/yyjson
   :alt: PyPI - License

.. image:: https://img.shields.io/pypi/v/yyjson
   :alt: PyPI - Version


Features
--------

- **Fast**: `yyjson` is several times faster than the builtin JSON module, and
  is `faster than most other JSON libraries <https://github.com/tktech/json_benchmark>`_.
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
- **SAX-style parsing**: An optional SAX-style parser allows you to read JSON
  documents of unlimited size with bounded memory usage, typically 2-5x
  faster than `ijson`.


Installation
------------

If binary wheels are available for your platform, you can install the latest
version of yyjson with pip:

    pip install yyjson

If you want to build from source, or if binary wheels aren't available, you'll
just need a C89 compiler, such as GCC or Clang.

Or you can install the latest development version from GitHub:

    pip install git+https://github.com/tktech/py_yyjson.git


Benchmarks
----------

You can find the benchmarks for this project in its sister project,
`json_benchmark`_. yyjson compares favourably, and often beats, most other
libraries.

Examples
--------

Parsing
^^^^^^^

Parse a JSON document from a file:

.. code-block:: python

    >>> from pathlib import Path
    >>> from yyjson import Document
    >>> Document(Path("canada.json")).as_obj
    {'type': 'FeatureCollection', 'features': [...], 'bbox': [...], 'crs': {...}}


Parse a JSON document from a string:

.. code-block:: python

    >>> from yyjson import Document
    >>> Document('{"hello": "world"}').as_obj
    {'hello': 'world'}

Parse a JSON document from a bytes object:

.. code-block:: python

    >>> from yyjson import Document
    >>> Document(b'{"hello": "world"}').as_obj
    {'hello': 'world'}


Load part of a document
^^^^^^^^^^^^^^^^^^^^^^^

When you only need a small part of a document, you can use a JSON Pointer to
extract just the part you actually need. This can be a massive performance
improvement when working with large JSON documents, as most of the time
spent parsing JSON in Python is spent just creating the Python objects!

.. code-block:: python

    >>> from pathlib import Path
    >>> from yyjson import Document
    >>> doc = Document(Path("canada.json"))
    >>> features = doc.get_pointer("/features")


Patch a document
^^^^^^^^^^^^^^^^

JSON manipulation operations are supported, such as
`JSON Merge-Patch <https://tools.ietf.org/html/rfc7386>`_ and
`JSON Patch <https://tools.ietf.org/html/rfc6902>`_. These operations
allow you to manipulate JSON documents without deserializing them into
Python objects at all.

For example, lets add an entry to a GeoJSON file without deserializing
the entire document into Python objects using JSON Patch:

.. code-block:: python

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

Serialize a Python object to JSON:

.. code-block:: python

    >>> from yyjson import Document
    >>> doc = Document({
    ...   "hello": "world",
    ...   "foo": [1, 2, 3],
    ...   "bar": {"a": 1, "b": 2}
    ... })
    >>> doc.dumps()
    '{"hello":"world","foo":[1,2,3],"bar":{"a":1,"b":2}}'


Customizing JSON Reading & Writing
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You can customize the JSON reading and writing process using
:class:`yyjson.ReaderFlags` and :class:`yyjson.WriterFlags`. For example
if we wanted to allow comments and trailing commas, we could do:


.. code-block:: python

    >>> from yyjson import Document, ReaderFlags, WriterFlags
    >>> doc = Document(
    ...   '{"hello": "world",} // This is a comment',
    ...   flags=ReaderFlags.ALLOW_COMMENTS | ReaderFlags.ALLOW_TRAILING_COMMAS
    ... )


Likewise we can customize the writing process:


.. code-block:: python

    >>> from yyjson import Document, ReaderFlags, WriterFlags
    >>> doc = Document({
    ...   "hello": "world"
    ... })
    >>> doc.dumps(flags=WriterFlags.PRETTY_TWO_SPACES)


Reading Huge Numbers
^^^^^^^^^^^^^^^^^^^^

If you're reading huge floats/doubles or require perfect precision, you can
tell yyjson to read them as Decimals:

.. code-block:: python

    >>> from yyjson import Document, ReaderFlags
    >>> float('1.7976931348623157e+310')
    inf
    >>> doc = Document(
    ...   '{"huge": 1.7976931348623157e+310}',
    ...   flags=ReaderFlags.NUMBERS_AS_DECIMAL
    ... )
    >>> print(doc.get_pointer('/huge'))
    1.7976931348623157E+310


Or use ``ReaderFlags.BIG_NUMBERS_AS_DECIMAL`` to only read numbers that are
too large for Python's float type as Decimals:

.. code-block:: python

    >>> from yyjson import Document, ReaderFlags
    >>> doc = Document(
        '{"huge": 1.7976931348623157e+310, "small": 1.0}',
        flags=ReaderFlags.BIG_NUMBERS_AS_DECIMAL
    )
    >>> type(doc.get_pointer('/huge'))
    <class 'decimal.Decimal'>
    >>> type(doc.get_pointer('/small'))
    <class 'float'>


Streaming (SAX) parsing
^^^^^^^^^^^^^^^^^^^^^^^

Some documents are too large to hold in memory, and sometimes you only need
a few values out of a huge file. :func:`yyjson.sax` parses JSON as a stream
of events with bounded memory: peak usage is a fixed sliding window
(``window_size``, 256 KiB by default) plus the nesting depth, no matter how
large the input is. The source can be a ``str``, ``bytes``, a
``pathlib.Path``, or a binary file-like object.

The handler is any object; each of the following methods is called if it
exists, and every one of them is optional:

* ``obj_begin()`` / ``obj_end(count)`` -- an object opened or closed;
  ``count`` is the number of members.
* ``arr_begin()`` / ``arr_end(count)`` -- an array opened or closed;
  ``count`` is the number of elements.
* ``key(value)`` -- an object member key.
* ``string(value)`` -- a string value.
* ``number(value)`` -- an ``int``, ``float``, or (with the Decimal reader
  flags) ``Decimal`` value.
* ``boolean(value)`` / ``null()``.

For example, collecting every key used anywhere in a document:

.. code-block:: python

    >>> from yyjson import sax
    >>> class KeyCollector:
    ...     def __init__(self):
    ...         self.keys = set()
    ...     def key(self, value):
    ...         self.keys.add(value)
    ...
    >>> collector = KeyCollector()
    >>> sax('{"a": 1, "b": {"c": [1, 2]}}', collector)
    >>> sorted(collector.keys)
    ['a', 'b', 'c']

A handler method can return ``False`` to stop parsing early -- for example
once you've found the value you were looking for -- and ``sax()`` returns
normally. The reader flags are honored, so combining ``sax()`` with
``ReaderFlags.NUMBERS_AS_DECIMAL`` streams perfect-precision numbers.

The one constraint is that a single string or number token cannot be larger
than the window; such an input raises ``ValueError``.


.. _yyjson: https://github.com/ibireme/yyjson
.. _json_benchmark: https://github.com/tktech/json_benchmark