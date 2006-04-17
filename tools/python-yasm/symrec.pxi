# Python bindings for Yasm: Pyrex input file for symrec.h
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

cdef extern from "libyasm/symrec.h":
    cdef yasm_symtab* yasm_symtab_create()
    cdef void yasm_symtab_destroy(yasm_symtab *symtab)
    cdef yasm_symrec* yasm_symtab_use(yasm_symtab *symtab, char *name,
            unsigned long line)
    cdef yasm_symrec* yasm_symtab_get(yasm_symtab *symtab, char *name)
    cdef yasm_symrec* yasm_symtab_define_equ(yasm_symtab *symtab,
            char *name, yasm_expr *e, unsigned long line)
    cdef yasm_symrec* yasm_symtab_define_label(yasm_symtab *symtab,
            char *name, yasm_bytecode *precbc, int in_table, unsigned long line)
    cdef yasm_symrec* yasm_symtab_define_curpos(yasm_symtab *symtab, char *name,
            yasm_bytecode *precbc, unsigned long line)
    cdef yasm_symrec* yasm_symtab_define_special(yasm_symtab *symtab,
            char *name, yasm_sym_vis vis)
    cdef yasm_symrec* yasm_symtab_declare(yasm_symtab *symtab,
            char *name, yasm_sym_vis vis, unsigned long line)
    cdef void yasm_symrec_declare(yasm_symrec *symrec, yasm_sym_vis vis,
            unsigned long line)

    ctypedef int (*yasm_symtab_traverse_callback)(yasm_symrec *sym, void *d)
    cdef int yasm_symtab_traverse(yasm_symtab *symtab, void *d,
            yasm_symtab_traverse_callback func)

    ctypedef struct yasm_symtab_iter
    cdef yasm_symtab_iter *yasm_symtab_first(yasm_symtab *symtab)
    cdef yasm_symtab_iter *yasm_symtab_next(yasm_symtab_iter *prev)
    cdef yasm_symrec *yasm_symtab_iter_value(yasm_symtab_iter *cur)

    cdef void yasm_symtab_parser_finalize(yasm_symtab *symtab,
            int undef_extern, yasm_objfmt *objfmt)
    cdef void yasm_symtab_print(yasm_symtab *symtab, FILE *f, int indent_level)
    cdef char* yasm_symrec_get_name(yasm_symrec *sym)
    cdef yasm_sym_vis yasm_symrec_get_visibility(yasm_symrec *sym)
    cdef yasm_expr* yasm_symrec_get_equ(yasm_symrec *sym)

    ctypedef yasm_bytecode *yasm_symrec_get_label_bytecodep

    cdef int yasm_symrec_get_label(yasm_symrec *sym,
            yasm_symrec_get_label_bytecodep *precbc)
    cdef int yasm_symrec_is_special(yasm_symrec *sym)
    cdef void* yasm_symrec_get_data(yasm_symrec *sym,
            yasm_assoc_data_callback *callback)
    cdef void yasm_symrec_add_data(yasm_symrec *sym,
            yasm_assoc_data_callback *callback, void *data)
    cdef void yasm_symrec_print(yasm_symrec *sym, FILE *f, int indent_level)

cdef class Symbol:
    cdef yasm_symrec *sym

    def __new__(self, symrec):
        self.sym = NULL
        if PyCObject_Check(symrec):  # should check Desc
            self.sym = <yasm_symrec *>PyCObject_AsVoidPtr(symrec)
        else:
            raise NotImplementedError

    # no deref or destroy necessary

    def get_name(self): return yasm_symrec_get_name(self.sym)
    def get_visibility(self): return yasm_symrec_get_visibility(self.sym)
    def get_equ(self):
        return __make_expression(yasm_expr_copy(yasm_symrec_get_equ(self.sym)))
    def get_label(self):
        cdef yasm_symrec_get_label_bytecodep bc
        if yasm_symrec_get_label(self.sym, &bc): pass # TODO
            #return Bytecode(bc)
        else: raise TypeError("Symbol '%s' is not a label" % self.get_name())

    def get_is_special(self):
        return yasm_symrec_is_special(self.sym)

    def get_data(self): pass # TODO
        #return <object>(yasm_symrec_get_data(self.sym, PyYasmAssocData))

    def set_data(self, data): pass # TODO
        #yasm_symrec_set_data(self.sym, PyYasmAssocData, data)

#
# Use associated data mechanism to keep Symbol reference paired with symrec.
#

cdef class __assoc_data_callback:
    cdef yasm_assoc_data_callback *cb
    def __new__(self, destroy, print_):
        self.cb = <yasm_assoc_data_callback *>malloc(sizeof(yasm_assoc_data_callback))
        self.cb.destroy = <void (*) (void *)>PyCObject_AsVoidPtr(destroy)
        self.cb.print_ = <void (*) (void *, FILE *, int)>PyCObject_AsVoidPtr(print_)
    def __dealloc__(self):
        free(self.cb)

cdef void __python_symrec_cb_destroy(void *data):
    Py_DECREF(<object>data)
cdef void __python_symrec_cb_print(void *data, FILE *f, int indent_level):
    pass
__python_symrec_cb = __assoc_data_callback(
        PyCObject_FromVoidPtr(&__python_symrec_cb_destroy, NULL),
        PyCObject_FromVoidPtr(&__python_symrec_cb_print, NULL))

cdef object __make_symbol(yasm_symrec *symrec):
    cdef void *data
    data = yasm_symrec_get_data(symrec,
                                (<__assoc_data_callback>__python_symrec_cb).cb)
    if data != NULL:
        return <object>data
    symbol = Symbol(PyCObject_FromVoidPtr(symrec, NULL))
    yasm_symrec_add_data(symrec,
                         (<__assoc_data_callback>__python_symrec_cb).cb,
                         <void *>symbol)
    Py_INCREF(symbol)       # We're keeping a reference on the C side!
    return symbol

cdef class Bytecode
cdef class SymbolTable

cdef class SymbolTableKeyIterator:
    cdef yasm_symtab_iter *iter

    def __new__(self, symtab):
        if not isinstance(symtab, SymbolTable):
            raise TypeError
        self.iter = yasm_symtab_first((<SymbolTable>symtab).symtab)

    def __iter__(self):
        return self

    def __next__(self):
        if self.iter == NULL:
            raise StopIteration
        rv = yasm_symrec_get_name(yasm_symtab_iter_value(self.iter))
        self.iter = yasm_symtab_next(self.iter)
        return rv

cdef class SymbolTableValueIterator:
    cdef yasm_symtab_iter *iter

    def __new__(self, symtab):
        if not isinstance(symtab, SymbolTable):
            raise TypeError
        self.iter = yasm_symtab_first((<SymbolTable>symtab).symtab)

    def __iter__(self):
        return self

    def __next__(self):
        if self.iter == NULL:
            raise StopIteration
        rv = __make_symbol(yasm_symtab_iter_value(self.iter))
        self.iter = yasm_symtab_next(self.iter)
        return rv

cdef class SymbolTableItemIterator:
    cdef yasm_symtab_iter *iter

    def __new__(self, symtab):
        if not isinstance(symtab, SymbolTable):
            raise TypeError
        self.iter = yasm_symtab_first((<SymbolTable>symtab).symtab)

    def __iter__(self):
        return self

    def __next__(self):
        cdef yasm_symrec *sym
        if self.iter == NULL:
            raise StopIteration
        sym = yasm_symtab_iter_value(self.iter)
        rv = (yasm_symrec_get_name(sym), __make_symbol(sym))
        self.iter = yasm_symtab_next(self.iter)
        return rv

cdef class SymbolTable:
    cdef yasm_symtab *symtab

    def __new__(self):
        self.symtab = yasm_symtab_create()

    def __dealloc__(self):
        if self.symtab != NULL: yasm_symtab_destroy(self.symtab)

    def use(self, name, line):
        return __make_symbol(yasm_symtab_use(self.symtab, name, line))

    def define_equ(self, name, expr, line):
        if not isinstance(expr, Expression):
            raise TypeError
        return __make_symbol(yasm_symtab_define_equ(self.symtab, name,
                (<Expression>expr).expr, line))

    def define_label(self, name, precbc, in_table, line):
        if not isinstance(precbc, Bytecode):
            raise TypeError
        return __make_symbol(yasm_symtab_define_label(self.symtab, name,
                (<Bytecode>precbc).bc, in_table, line))

    def define_special(self, name, vis):
        return __make_symbol(yasm_symtab_define_special(self.symtab, name,
                                                        vis))

    def declare(self, name, vis, line):
        return __make_symbol(yasm_symtab_declare(self.symtab, name, vis, line))

    #
    # Methods to make SymbolTable behave like a dictionary of Symbols.
    #

    def __getitem__(self, key):
        cdef yasm_symrec *symrec
        symrec = yasm_symtab_get(self.symtab, key)
        if symrec == NULL:
            raise KeyError
        return __make_symbol(symrec)

    def __contains__(self, key):
        cdef yasm_symrec *symrec
        symrec = yasm_symtab_get(self.symtab, key)
        return symrec != NULL

    def keys(self):
        cdef yasm_symtab_iter *iter
        l = []
        iter = yasm_symtab_first(self.symtab)
        while iter != NULL:
            l.append(yasm_symrec_get_name(yasm_symtab_iter_value(iter)))
            iter = yasm_symtab_next(iter)
        return l

    def values(self):
        cdef yasm_symtab_iter *iter
        l = []
        iter = yasm_symtab_first(self.symtab)
        while iter != NULL:
            l.append(__make_symbol(yasm_symtab_iter_value(iter)))
            iter = yasm_symtab_next(iter)
        return l

    def items(self):
        cdef yasm_symtab_iter *iter
        cdef yasm_symrec *sym
        l = []
        iter = yasm_symtab_first(self.symtab)
        while iter != NULL:
            sym = yasm_symtab_iter_value(iter)
            l.append((yasm_symrec_get_name(sym), __make_symbol(sym)))
            iter = yasm_symtab_next(iter)
        return l

    def has_key(self, key):
        cdef yasm_symrec *symrec
        symrec = yasm_symtab_get(self.symtab, key)
        return symrec != NULL

    def get(self, key, x):
        cdef yasm_symrec *symrec
        symrec = yasm_symtab_get(self.symtab, key)
        if symrec == NULL:
            return x
        return __make_symbol(symrec)

    def iterkeys(self): return SymbolTableKeyIterator(self)
    def itervalues(self): return SymbolTableValueIterator(self)
    def iteritems(self): return SymbolTableItemIterator(self)

    # This doesn't follow Python's guideline to make this iterkeys() for
    # mappings, but makes more sense in this context, e.g. for sym in symtab.
    def __iter__(self): return SymbolTableValueIterator(self)

