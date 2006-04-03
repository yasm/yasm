cdef extern from "libyasm/value.h":
    cdef void yasm_value_initialize(yasm_value *value, yasm_expr *e)
    cdef void yasm_value_init_sym(yasm_value *value, yasm_symrec *sym)
    cdef void yasm_value_delete(yasm_value *value)
    cdef int yasm_value_finalize(yasm_value *value)
    cdef int yasm_value_finalize_expr(yasm_value *value, yasm_expr *e)
    cdef int yasm_value_output_basic(yasm_value *value, unsigned char *buf,
            size_t destsize, size_t valsize, int shift, yasm_bytecode *bc,
            int warn, yasm_arch *arch, yasm_calc_bc_dist_func calc_bc_dist)
    cdef void yasm_value_print(yasm_value *value, FILE *f, int indent_level)

cdef class Value:
    cdef yasm_value value
    def __new__(self, value=None):
        yasm_value_initialize(&self.value, NULL)
        if value is None:
            pass
        elif isinstance(value, Expression):
            yasm_value_initialize(&self.value, (<Expression>value).expr)
            (<Expression>value).expr = NULL
        #elif isinstance(value, Symbol):
        else:
            raise ValueError("Invalid value type '%s'" % type(value))

    def __dealloc__(self):
        yasm_value_delete(&self.value)
