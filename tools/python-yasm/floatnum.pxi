# Python bindings for Yasm: Pyrex input file for floatnum.h
#
#  Copyright (C) 2006  Michael Urman, Peter Johnson
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

cdef extern from "libyasm/floatnum.h":
    cdef void yasm_floatnum_initialize()
    cdef void yasm_floatnum_cleanup()
    cdef yasm_floatnum* yasm_floatnum_create(char *str)
    cdef yasm_floatnum* yasm_floatnum_copy(yasm_floatnum *flt)
    cdef void yasm_floatnum_destroy(yasm_floatnum *flt)
    cdef void yasm_floatnum_calc(yasm_floatnum *acc, yasm_expr_op op,
            yasm_floatnum *operand, size_t line)
    cdef int yasm_floatnum_get_int(yasm_floatnum *flt, size_t *ret_val)
    cdef int yasm_floatnum_get_sized(yasm_floatnum *flt, unsigned char *ptr,
            size_t destsize, size_t valsize, size_t shift, int
            bigendian, int warn, size_t line)
    cdef int yasm_floatnum_check_size(yasm_floatnum *flt, size_t size)
    cdef void yasm_floatnum_print(yasm_floatnum *flt, FILE *f)

cdef class FloatNum:
    cdef yasm_floatnum *flt
    def __new__(self, value):
        self.flt = NULL
        if isinstance(value, FloatNum):
            self.flt = yasm_floatnum_copy((<FloatNum>value).flt)
            return
        if PyCObject_Check(value):  # should check Desc
            self.flt = <yasm_floatnum *>PyCObject_AsVoidPtr(value)
            return

        if isinstance(value, float): string = str(float)
        else: string = value
        self.flt = yasm_floatnum_create(string)

    def __dealloc__(self):
        if self.flt != NULL: yasm_floatnum_destroy(self.flt)

    def __neg__(self):
        result = FloatNum(self)
        yasm_floatnum_calc((<FloatNum>result).flt, YASM_EXPR_NEG, NULL, 0)
        return result
    def __pos__(self): return self

