

cdef extern from *:
  ctypedef unsigned int __c_size_t "size_t"

cdef extern from "object.h":
  ctypedef class __builtin__.type [object PyHeapTypeObject]:
    pass

cdef class Meta(type):
  pass

cdef class _CObject:
#cdef public class _CObject [ object AdaptObj_CObject, type AdaptType_CObject ]:
  cdef void*p
  cdef int malloced
  cdef public object incref # incref what we are pointing to

cdef class _CPointer(_CObject):
  pass

cdef class _CArray(_CPointer):
  cdef public __c_size_t el_size # sizeof what we point to # XX class attr ??
  cdef __c_size_t nmemb
  cdef void *items

cdef class _CChar(_CObject):
  pass
cdef class _CSChar(_CObject):
  pass
cdef class _CUChar(_CObject):
  pass
cdef class _CShort(_CObject):
  pass
cdef class _CUShort(_CObject):
  pass
cdef class _CInt(_CObject):
  pass
cdef class _CUInt(_CObject):
  pass
cdef class _CLong(_CObject):
  pass
cdef class _CULong(_CObject):
  pass
cdef class _CLLong(_CObject):
  pass
cdef class _CULLong(_CObject):
  pass
cdef class _CFloat(_CObject):
  pass
cdef class _CDouble(_CObject):
  pass
cdef class _CLDouble(_CObject):
  pass
cdef class _CStruct(_CObject):
  pass
