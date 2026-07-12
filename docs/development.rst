Development
===========

To build the extension and install development requirements, use:

  pip install -e ".[test]"

Keep in mind since this is a C extension, you'll need a C compiler installed.
On Windows, you can use Visual Studio Community Edition. On Linux and macOS,
you can use GCC or Clang. You'll also need to re-run the install each time
you make a change to a ``.c`` or ``.h`` file to recompile.

To run the tests, just type ``pytest``. To prepare for a release or to rebuild
documentation, you need a few extra dependencies:

    pip install -e ".[release]"

You can then rebuild the documentation with the same strict build CI uses:

    sphinx-build -W -n -b html docs docs/_build/html


Profiling
---------

Performance is a core feature of this project, not an afterthought - being
several times faster than the builtin ``json`` module is most of the reason
py_yyjson exists. Changes to the hot paths (parsing, DOM-to-Python
conversion, and serialization) should come with before/after numbers, and
"obvious" improvements should be measured rather than assumed: more than one
past optimization turned out to be a wash once benchmarked.

Benchmarks intentionally live outside of this repository in
`json_benchmark <https://github.com/tktech/json_benchmark>`_, which compares
py_yyjson against other popular JSON libraries with a single harness and a
corpus of real-world documents. If you're proposing a performance change,
run it before and after.
