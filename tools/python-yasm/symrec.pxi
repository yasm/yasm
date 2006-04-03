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

cdef object __make_symbol(yasm_symrec *symrec):
    return Symbol(PyCObject_FromVoidPtr(symrec, NULL))

cdef class Bytecode

cdef class SymbolTable:
    cdef yasm_symtab *symtab
    def __new__(self):
        self.symtab = yasm_symtab_create()

    def __dealloc__(self):
        if self.symtab != NULL: yasm_symtab_destroy(self.symtab)

    def use(self, name, line):
        return __make_symbol(yasm_symtab_use(self.symtab, name, line))

    def define_equ(self, name, expr, line):
        return __make_symbol(yasm_symtab_define_equ(self.symtab, name,
                (<Expression>expr).expr, line))

    def define_label(self, name, precbc, in_table, line):
        return __make_symbol(yasm_symtab_define_label(self.symtab, name,
                (<Bytecode>precbc).bc, in_table, line))

    def define_special(self, name, vis):
        return __make_symbol(yasm_symtab_define_special(self.symtab, name, vis))

    def declare(self, name, vis, line):
        return __make_symbol(yasm_symtab_declare(self.symtab, name, vis, line))

    #def traverse(self, func, *args, **kw):
    #    return yasm_symtab_traverse(self.symtab, (args, kw), func)

    #def parser_finalize(self, undef_extern, objfmt):
    #    yasm_symtab_parser_finalize(self.symtab, undef_extern,
    #            (<ObjectFormat>objfmt).objfmt)
    #


