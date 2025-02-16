API
---

.. note::

    When creating and manipulating a :class:`Document`, it's important to keep
    in mind how the underlying yyjson library manages memory. If you modify a
    Document, such as removing or replacing a value, the memory for that value
    is not freed until the Document is destroyed. A Document isn't meant to be
    kept around and constantly modified, so don't use it as, say, a database.


.. testsetup:: *

    from yyjson import Document, ReaderFlags, WriterFlags

.. automodule:: yyjson
   :members:
   :undoc-members:
   :show-inheritance:

