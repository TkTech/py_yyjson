[py]yyjson
==========

Python bindings to the fantastic `yyjson`_ project. This module provides a fast,
flexible, and portable JSON parser and serializer. yyjson by default has strict
compliance to the specification, but also includes options to parse JSON with
comments, NaN, Inf, integers of any arbitrary size, and more.

Binary packages are provided for many versions of Python on many architectures,
and only requires a C89-compliant compiler when building from source.

Why use it?
-----------

Used as a drop-in replacement for the builtin JSON module, yyjson is several
times faster. However, most of the time spent loading a JSON document in Python
isn't the parsing, it's the creation of Python objects. [py]yyjson can provide
significant speedups by avoiding creating Python objects for the entire
document, allowing you to extract just the parts of the document you
actually care about. It also provides facilities for manipulating the
document in native code, such as performing a JSON Merge-Patch (RFC 7386).


.. note::

    When creating and manipulating a :class:`Document`, it's important to keep
    in mind how the underlying yyjson library manages memory. If you modify a
    Document, such as removing or replacing a value, the memory for that value
    is not freed until the Document is destroyed. A Document isn't meant to be
    kept around and constantly modified, so don't use it as, say, a database.


Benchmarks
----------

You can find the benchmarks for this project in its sister project,
`json_benchmark`_. yyjson compares favourably, and often beats, most other
libraries.

API
---

.. testsetup:: *

    from yyjson import Document, ReaderFlags, WriterFlags

.. automodule:: yyjson
   :members:
   :undoc-members:
   :show-inheritance:

.. _yyjson: https://github.com/ibireme/yyjson
.. _json_benchmark: https://github.com/tktech/json_benchmark