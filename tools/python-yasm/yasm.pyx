cdef extern from "Python.h":
    cdef object PyCObject_FromVoidPtr(void *cobj, void (*destr)(void *))
    cdef object PyCObject_FromVoidPtrAndDesc(void *cobj, void *desc,
                                             void (*destr)(void *, void *))
    cdef int PyCObject_Check(object)
    cdef void *PyCObject_AsVoidPtr(object)
    cdef void *PyCObject_GetDesc(object)

cdef extern from "libyasm/compat-queue.h":
    pass

cdef class Register:
    cdef unsigned long reg
    def __new__(self, reg):
        self.reg = reg

include "coretype.pxi"

include "intnum.pxi"
include "floatnum.pxi"
include "expr.pxi"
include "symrec.pxi"
include "value.pxi"

include "bytecode.pxi"


cdef extern from "libyasm/bitvect.h":
    cdef void BitVector_Boot()
    cdef void BitVector_Shutdown()

def __initialize():
    BitVector_Boot()
    yasm_intnum_initialize()
    yasm_floatnum_initialize()

def __cleanup():
    yasm_floatnum_cleanup()
    yasm_intnum_cleanup()
    BitVector_Shutdown()

__initialize()
import atexit
atexit.register(__cleanup)

