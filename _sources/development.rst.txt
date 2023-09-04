Development
===========

To install development requirements, use:

  pip install -e ".[test]"

You can then build the project using ``python setup.py develop``. Keep in
mind since this is a C extension, you'll need a C compiler installed. On
Windows, you can use Visual Studio Community Edition. On Linux, you can
use GCC or Clang. You'll also need to re-run it each time you make a
change to a ``.c`` or ``.h`` file to recompile.

To run the tests, just type ``pytest``. To prepare for a release or to rebuild
documentation, you need a few extra dependencies:

    pip install -e ".[release]"

You can then rebuild the documentation by running ``make html`` within the
``docs/`` directory.


Why not Poetry?
---------------

Poetry is a great tool, but it's not a good fit for this project. Poetry
is designed to manage Python projects, and this project is a C extension
with a Python wrapper. Poetry's current support for building C extensions
is a hack which may change at any time.