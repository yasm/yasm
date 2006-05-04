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
    cdef yasm_intnum *yasm_intnum_create_dec(char *str)
    cdef yasm_intnum *yasm_intnum_create_bin(char *str)
    cdef yasm_intnum *yasm_intnum_create_oct(char *str)
    cdef yasm_intnum *yasm_intnum_create_hex(char *str)
    cdef yasm_intnum *yasm_intnum_create_charconst_nasm(char *str)
    cdef yasm_intnum *yasm_intnum_create_uint(unsigned long i)
    cdef yasm_intnum *yasm_intnum_create_int(long i)
    cdef yasm_intnum *yasm_intnum_create_leb128(unsigned char *ptr,
            int sign, unsigned long *size)
    cdef yasm_intnum *yasm_intnum_create_sized(unsigned char *ptr, int sign,
            size_t srcsize, int bigendian)
    cdef yasm_intnum *yasm_intnum_copy(yasm_intnum *intn)
    cdef void yasm_intnum_destroy(yasm_intnum *intn)
    cdef void yasm_intnum_calc(yasm_intnum *acc, yasm_expr_op op,
            yasm_intnum *operand)
    cdef void yasm_intnum_zero(yasm_intnum *intn)
    cdef void yasm_intnum_set_uint(yasm_intnum *intn, unsigned long val)
    cdef int yasm_intnum_is_zero(yasm_intnum *acc)
    cdef int yasm_intnum_is_pos1(yasm_intnum *acc)
    cdef int yasm_intnum_is_neg1(yasm_intnum *acc)
    cdef int yasm_intnum_sign(yasm_intnum *acc)
    cdef unsigned long yasm_intnum_get_uint(yasm_intnum *intn)
    cdef long yasm_intnum_get_int(yasm_intnum *intn)
    cdef void yasm_intnum_get_sized(yasm_intnum *intn, unsigned char *ptr,
	    size_t destsize, size_t valsize, int shift, int bigendian, int warn)
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

cdef class IntNum

cdef object __intnum_op(object x, yasm_expr_op op, object y):
    if isinstance(x, IntNum):
        result = IntNum(x)
        if y is None:
            yasm_intnum_calc((<IntNum>result).intn, op, NULL)
        else:
            # Coerce to intnum if not already
            if isinstance(y, IntNum):
                rhs = y
            else:
                rhs = IntNum(y)
            yasm_intnum_calc((<IntNum>result).intn, op, (<IntNum>rhs).intn)
        return result
    elif isinstance(y, IntNum):
        # Reversed operation - x OP y still, just y is intnum, x isn't.
        result = IntNum(x)
        yasm_intnum_calc((<IntNum>result).intn, op, (<IntNum>y).intn)
        return result
    else:
        raise NotImplementedError

cdef object __make_intnum(yasm_intnum *intn):
    return IntNum(PyCObject_FromVoidPtr(intn, NULL))

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
        self.intn = yasm_intnum_create_sized(buf, 1, 16, 0)

    def __dealloc__(self):
        if self.intn != NULL: yasm_intnum_destroy(self.intn)

    def __long__(self):
        cdef unsigned char buf[16]
        yasm_intnum_get_sized(self.intn, buf, 16, 128, 0, 0, 0)
        return _PyLong_FromByteArray(buf, 16, 1, 1)

    def __repr__(self):
        return "IntNum(%d)" % self

    def __int__(self): return int(self.__long__())
    def __complex__(self): return complex(self.__long__())
    def __float__(self): return float(self.__long__())

    def __oct__(self): return oct(int(self.__long__()))
    def __hex__(self): return hex(int(self.__long__()))

    def __add__(x, y): return __intnum_op(x, YASM_EXPR_ADD, y)
    def __sub__(x, y): return __intnum_op(x, YASM_EXPR_SUB, y)
    def __mul__(x, y): return __intnum_op(x, YASM_EXPR_MUL, y)
    def __div__(x, y): return __intnum_op(x, YASM_EXPR_SIGNDIV, y)
    def __floordiv__(x, y): return __intnum_op(x, YASM_EXPR_SIGNDIV, y)
    def __mod__(x, y): return __intnum_op(x, YASM_EXPR_SIGNMOD, y)
    def __neg__(self): return __intnum_op(self, YASM_EXPR_NEG, None)
    def __pos__(self): return self
    def __abs__(self):
        if yasm_intnum_sign(self.intn) >= 0: return self
        else: return __intnum_op(self, YASM_EXPR_NEG, None)
    def __nonzero__(self): return not yasm_intnum_is_zero(self.intn)
    def __invert__(self): return __intnum_op(self, YASM_EXPR_NOT, None)
    def __lshift__(x, y): return __intnum_op(x, YASM_EXPR_SHL, y)
    def __rshift__(x, y): return __intnum_op(x, YASM_EXPR_SHR, y)
    def __and__(x, y): return __intnum_op(x, YASM_EXPR_AND, y)
    def __or__(x, y): return __intnum_op(x, YASM_EXPR_OR, y)
    def __xor__(x, y): return __intnum_op(x, YASM_EXPR_XOR, y)

    cdef object __op(self, yasm_expr_op op, object x):
        if isinstance(x, IntNum):
            rhs = x
        else:
            rhs = IntNum(x)
        yasm_intnum_calc(self.intn, op, (<IntNum>rhs).intn)
        return self

    def __iadd__(self, x): return self.__op(YASM_EXPR_ADD, x)
    def __isub__(self, x): return self.__op(YASM_EXPR_SUB, x)
    def __imul__(self, x): return self.__op(YASM_EXPR_MUL, x)
    def __idiv__(self, x): return self.__op(YASM_EXPR_SIGNDIV, x)
    def __ifloordiv__(self, x): return self.__op(YASM_EXPR_SIGNDIV, x)
    def __imod__(self, x): return self.__op(YASM_EXPR_MOD, x)
    def __ilshift__(self, x): return self.__op(YASM_EXPR_SHL, x)
    def __irshift__(self, x): return self.__op(YASM_EXPR_SHR, x)
    def __iand__(self, x): return self.__op(YASM_EXPR_AND, x)
    def __ior__(self, x): return self.__op(YASM_EXPR_OR, x)
    def __ixor__(self, x): return self.__op(YASM_EXPR_XOR, x)

    def __cmp__(self, x):
        cdef yasm_intnum *t
        t = yasm_intnum_copy(self.intn)
        if isinstance(x, IntNum):
            rhs = x
        else:
            rhs = IntNum(x)
        yasm_intnum_calc(t, YASM_EXPR_SUB, (<IntNum>rhs).intn)
        result = yasm_intnum_sign(t)
        yasm_intnum_destroy(t)
        return result

    def __richcmp__(x, y, op):
        cdef yasm_expr_op aop
        if op == 0: aop = YASM_EXPR_LT
        elif op == 1: aop = YASM_EXPR_LE
        elif op == 2: aop = YASM_EXPR_EQ
        elif op == 3: aop = YASM_EXPR_NE
        elif op == 4: aop = YASM_EXPR_GT
        elif op == 5: aop = YASM_EXPR_GE
        else: raise NotImplementedError
        v = __intnum_op(x, aop, y)
        return bool(not yasm_intnum_is_zero((<IntNum>v).intn))
