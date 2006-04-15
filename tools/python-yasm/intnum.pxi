# Python bindings for Yasm: Pyrex input file for intnum.h
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

cdef extern from "libyasm/intnum.h":
    cdef void yasm_intnum_initialize()
    cdef void yasm_intnum_cleanup()
    cdef yasm_intnum *yasm_intnum_create_dec(char *str, unsigned long line)
    cdef yasm_intnum *yasm_intnum_create_bin(char *str, unsigned long line)
    cdef yasm_intnum *yasm_intnum_create_oct(char *str, unsigned long line)
    cdef yasm_intnum *yasm_intnum_create_hex(char *str, unsigned long line)
    cdef yasm_intnum *yasm_intnum_create_charconst_nasm(char *str,
            unsigned long line)
    cdef yasm_intnum *yasm_intnum_create_uint(unsigned long i)
    cdef yasm_intnum *yasm_intnum_create_int(long i)
    cdef yasm_intnum *yasm_intnum_create_leb128(unsigned char *ptr,
            int sign, unsigned long *size, unsigned long line)
    cdef yasm_intnum *yasm_intnum_create_sized(unsigned char *ptr, int sign,
            size_t srcsize, int bigendian, unsigned long line)
    cdef yasm_intnum *yasm_intnum_copy(yasm_intnum *intn)
    cdef void yasm_intnum_destroy(yasm_intnum *intn)
    cdef void yasm_intnum_calc(yasm_intnum *acc, yasm_expr_op op,
            yasm_intnum *operand, unsigned long line)
    cdef void yasm_intnum_zero(yasm_intnum *intn)
    cdef void yasm_intnum_set_uint(yasm_intnum *intn, unsigned long val)
    cdef int yasm_intnum_is_zero(yasm_intnum *acc)
    cdef int yasm_intnum_is_pos1(yasm_intnum *acc)
    cdef int yasm_intnum_is_neg1(yasm_intnum *acc)
    cdef int yasm_intnum_sign(yasm_intnum *acc)
    cdef unsigned long yasm_intnum_get_uint(yasm_intnum *intn)
    cdef long yasm_intnum_get_int(yasm_intnum *intn)
    cdef void yasm_intnum_get_sized(yasm_intnum *intn, unsigned char *ptr,
	    size_t destsize, size_t valsize, int shift, int bigendian, int warn,
            unsigned long line)
    cdef int yasm_intnum_check_size(yasm_intnum *intn, size_t size,
            size_t rshift, int rangetype)
    cdef unsigned long yasm_intnum_get_leb128(yasm_intnum *intn,
            unsigned char *ptr, int sign)
    cdef unsigned long yasm_intnum_size_leb128(yasm_intnum *intn,
            int sign)
    cdef unsigned long yasm_get_sleb128(long v, unsigned char *ptr)
    cdef unsigned long yasm_size_sleb128(long v)
    cdef unsigned long yasm_get_uleb128(unsigned long v, unsigned char *ptr)
    cdef unsigned long yasm_size_uleb128(unsigned long v)
    cdef void yasm_intnum_print(yasm_intnum *intn, FILE *f)

cdef class IntNum:
    cdef yasm_intnum *intn

    def __new__(self, value, base=None):
        cdef unsigned char buf[16]

        self.intn = NULL

        if isinstance(value, IntNum):
            self.intn = yasm_intnum_copy((<IntNum>value).intn)
            return
        if PyCObject_Check(value):  # should check Desc
            self.intn = <yasm_intnum *>PyCObject_AsVoidPtr(value)
            return

        val = None
        if isinstance(value, str):
            val = long(value, base)
        elif isinstance(value, (int, long)):
            val = long(value)

        if val is None:
            raise ValueError

        _PyLong_AsByteArray(val, buf, 16, 1, 1)
        self.intn = yasm_intnum_create_sized(buf, 1, 16, 0, 0)

    def __dealloc__(self):
        if self.intn != NULL: yasm_intnum_destroy(self.intn)

    def __long__(self):
        cdef unsigned char buf[16]
        yasm_intnum_get_sized(self.intn, buf, 16, 128, 0, 0, 0, 0)
        return _PyLong_FromByteArray(buf, 16, 1, 1)

    def __int__(self):
        return int(self.__long__())

    def __repr__(self):
        return "IntNum(%s)" % str(int(self))

    def __op(self, op, o=None):
        lhs = IntNum(self)
        if o is None:
            yasm_intnum_calc((<IntNum>lhs).intn, op, NULL, 0)
        else:
            if isinstance(o, IntNum): rhs = o
            else: rhs = IntNum(o)
            yasm_intnum_calc((<IntNum>lhs).intn, op, (<IntNum>rhs).intn, 0)
        return lhs

    def __add__(IntNum self, o): return self.__op(YASM_EXPR_ADD, o)
    def __sub__(IntNum self, o): return self.__op(YASM_EXPR_SUB, o)
    def __mul__(IntNum self, o): return self.__op(YASM_EXPR_MUL, o)
    def __div__(IntNum self, o): return self.__op(YASM_EXPR_SIGNDIV, o)
    def __floordiv__(IntNum self, o): return self.__op(YASM_EXPR_SIGNDIV, o)
    def __mod__(IntNum self, o): return self.__op(YASM_EXPR_SIGNMOD, o)
    def __neg__(IntNum self): return self.__op(YASM_EXPR_NEG)
    def __pos__(IntNum self): return self
    def __abs__(IntNum self):
        if yasm_intnum_sign(self.intn) >= 0: return self
        else: return self.__op(YASM_EXPR_NEG)
    def __nonzero__(IntNum self): return not yasm_intnum_is_zero(self.intn)
    def __invert__(IntNum self): return self.__op(YASM_EXPR_NOT)
    #def __lshift__(IntNum self, o)
    #def __rshift__(IntNum self, o)
    def __and__(IntNum self, o): return self.__op(YASM_EXPR_AND, o)
    def __or__(IntNum self, o): return self.__op(YASM_EXPR_OR, o)
    def __xor__(IntNum self, o): return self.__op(YASM_EXPR_XOR, o)

cdef object __make_intnum(yasm_intnum *intn):
    return IntNum(PyCObject_FromVoidPtr(intn, NULL))

