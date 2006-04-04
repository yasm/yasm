# Python bindings for Yasm: Pyrex input file for value.h
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
            yasm_value_initialize(&self.value,
                                  yasm_expr_copy((<Expression>value).expr))
        elif isinstance(value, Symbol):
            yasm_value_init_sym(&self.value, (<Symbol>value).sym)
        else:
            raise ValueError("Invalid value type '%s'" % type(value))

    def __dealloc__(self):
        yasm_value_delete(&self.value)

    def finalize(self):
        return yasm_value_finalize(&self.value)

