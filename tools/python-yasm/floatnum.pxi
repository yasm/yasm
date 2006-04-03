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

    def __op(self, op, o):
        if isinstance(o, FloatNum): rhs = o
        else: rhs = FloatNum(o)
        lhs = FloatNum(self)
        yasm_floatnum_calc((<FloatNum>lhs).flt, op, (<FloatNum>rhs).flt, 0)
        return lhs

    def __add__(FloatNum self, o): return self.__op(YASM_EXPR_ADD, o)
    def __sub__(FloatNum self, o): return self.__op(YASM_EXPR_SUB, o)
    def __mul__(FloatNum self, o): return self.__op(YASM_EXPR_MUL, o)
    def __div__(FloatNum self, o): return self.__op(YASM_EXPR_SIGNDIV, o)
    def __floordiv__(FloatNum self, o): return self.__op(YASM_EXPR_SIGNDIV, o)
    def __mod__(FloatNum self, o): return self.__op(YASM_EXPR_SIGNMOD, o)
    def __neg__(FloatNum self): return self.__op(YASM_EXPR_NEG, o)
    def __pos__(FloatNum self): return self
    #def __abs__(FloatNum self): return self
    #def __nonzero__(FloatNum self): return not yasm_floatnum_is_zero(self.intn)
    def __invert__(FloatNum self): return self.__op(YASM_EXPR_NOT, o)
    #def __lshift__(FloatNum self, o)
    #def __rshift__(FloatNum self, o)
    def __and__(FloatNum self, o): return self.__op(YASM_EXPR_AND, o)
    def __or__(FloatNum self, o): return self.__op(YASM_EXPR_OR, o)
    def __xor__(FloatNum self, o): return self.__op(YASM_EXPR_XOR, o)

