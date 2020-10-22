__all__ = [
    'load',
    'loads',
    'Document',
]

from cyyjson import Document


def load(fp, *, cls=None, object_hook=None, parse_float=None, parse_int=None,
         parse_constant=None, object_pairs_hook=None, **kwargs):
    """A stub for yyjson.Document(). All arguments but `fp` are ignored."""
    return Document(fp.read()).as_obj


def loads(s, *, cls=None, object_hook=None, parse_float=None, parse_int=None,
          parse_constant=None, object_pairs_hook=None, **kwargs):
    """A stub for yyjson.Document(). All arguments but `s` are ignored."""
    return Document(s).as_obj
