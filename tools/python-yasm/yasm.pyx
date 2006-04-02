cdef extern struct FILE:
    int fileno

ctypedef unsigned int size_t

cdef extern from "libyasm.h":

    # libyasm/coretype.h
    cdef struct yasm_arch
    cdef struct yasm_preproc
    cdef struct yasm_parser
    cdef struct yasm_optimizer
    cdef struct yasm_objfmt
    cdef struct yasm_dbgfmt
    cdef struct yasm_listfmt
    cdef struct yasm_assoc_data_callback
    cdef struct yasm_bytecode
    cdef struct yasm_object
    cdef struct yasm_section
    cdef struct yasm_symtab
    cdef struct yasm_symrec
    cdef struct yasm_expr
    cdef struct yasm_intnum
    cdef struct yasm_floatnum
    cdef struct yasm_linemap
    cdef struct yasm_valparam
    cdef struct yasm_valparamhead
    cdef struct yasm_insn_operands

    ctypedef yasm_intnum*(*yasm_calc_bc_dist_func)(yasm_bytecode *precbc1,
            yasm_bytecode *precbc2)
    ctypedef int*(*yasm_output_expr_func)(yasm_expr **ep, unsigned char
            *buf, size_t destsize, size_t valsize, int shift, unsigned
            long offset, yasm_bytecode *bc, int rel, int warn, void *d)
    ctypedef int(*yasm_output_reloc_func)(yasm_symrec *sym,
            yasm_bytecode *bc, unsigned char *buf, size_t destsize,
            size_t valsize, int warn, void *d)

    ctypedef enum yasm_expr_op:
        YASM_EXPR_IDENT
        YASM_EXPR_ADD
        YASM_EXPR_SUB
        YASM_EXPR_MUL
        YASM_EXPR_DIV
        YASM_EXPR_SIGNDIV
        YASM_EXPR_MOD
        YASM_EXPR_SIGNMOD
        YASM_EXPR_NEG
        YASM_EXPR_NOT
        YASM_EXPR_OR
        YASM_EXPR_AND
        YASM_EXPR_XOR
        YASM_EXPR_NOR
        YASM_EXPR_SHL
        YASM_EXPR_SHR
        YASM_EXPR_LOR
        YASM_EXPR_LAND
        YASM_EXPR_LNOT
        YASM_EXPR_LT
        YASM_EXPR_GT
        YASM_EXPR_EQ
        YASM_EXPR_LE
        YASM_EXPR_GE
        YASM_EXPR_NE
        YASM_EXPR_NONNUM
        YASM_EXPR_SEG
        YASM_EXPR_WRT
        YASM_EXPR_SEGOFF

    ctypedef enum yasm_sym_vis:
        YASM_SYM_LOCAL
        YASM_SYM_GLOBAL
        YASM_SYM_COMMON
        YASM_SYM_EXTERN
        YASM_SYM_DLOCAL

    # libyasm/expr.h
    cdef enum yasm_symrec_relocate_action:
        ReplaceZero = 0
        ReplaceValue = 1
        ReplaceValueIfLocal = 2

    cdef struct yasm_expr
    cdef struct yasm__exprhead
    cdef struct yasm_expr__item
    ctypedef yasm_expr* (*yasm_expr_xform_func)(yasm_expr *e, void *d)

    cdef yasm_expr* yasm_expr_create(yasm_expr_op op, yasm_expr__item *a,
            yasm_expr__item *b, size_t line)
    cdef yasm_expr__item* yasm_expr_sym(yasm_symrec *sym)
    cdef yasm_expr__item* yasm_expr_expr(yasm_expr *expr)
    cdef yasm_expr__item* yasm_expr_int(yasm_intnum *intn)
    cdef yasm_expr__item* yasm_expr_float(yasm_floatnum *flt)
    cdef yasm_expr__item* yasm_expr_reg(size_t reg)
    cdef yasm_expr* yasm_expr_copy(yasm_expr *e)
    cdef void yasm_expr_destroy(yasm_expr *e)
    cdef int yasm_expr_is_op(yasm_expr *e, yasm_expr_op)
    cdef yasm_expr* yasm_expr__level_tree(yasm_expr *e, int fold_const,
            int_simplify_ident, yasm_calc_bc_dist_func calc_bc_dist,
            yasm_expr_xform_func expr_form_extra, void
            *expr_xform_extra_data, yasm__exprhead *eh)
    cdef yasm_symrec* yasm_expr_extract_symrec(yasm_expr **ep,
            yasm_symrec_relocate_action relocate_action,
            yasm_calc_bc_dist_func calc_bc_dist)
    cdef yasm_expr* yasm_expr_extract_seg(yasm_expr **ep)
    cdef yasm_expr* yasm_expr_extract_segoff(yasm_expr **ep)
    cdef yasm_expr* yasm_expr_extract_wrt(yasm_expr **ep)
    cdef yasm_expr* yasm_expr_extract_shr(yasm_expr **ep)
    cdef yasm_intnum* yasm_expr_get_intnum(yasm_expr **ep)
    cdef yasm_floatnum* yasm_expr_get_floatnum(yasm_expr **ep)
    cdef yasm_symrec* yasm_expr_get_symrec(yasm_expr **ep, int simplify)
    cdef size_t* yasm_expr_get_reg(yasm_expr **ep, int simplify)
    cdef void yasm_expr_print(yasm_expr *e, FILE *f)

    # libyasm/bytecode.h
    cdef struct yasm_effaddr
    cdef struct yasm_immval
    cdef struct yasm_dataval
    cdef struct yasm_datavalhead

    cdef enum yasm_bc_resolve_flags:
        ResolveNone = 0
        ResolveError = 1 << 0
        ResolveMinLen = 1 << 1
        ResolveUnknownLen = 1 << 2

    cdef yasm_immval* yasm_imm_create_int(size_t int_val,
            size_t line)
    cdef yasm_immval* yasm_imm_create_expr(yasm_expr *e)
    cdef yasm_expr* yasm_ea_get_disp(yasm_effaddr *ea)
    cdef void yasm_ea_set_len(yasm_effaddr *ea, unsigned int len)
    cdef void yasm_ea_set_nosplit(yasm_effaddr *ea, unsigned int nosplit)
    cdef void yasm_ea_set_strong(yasm_effaddr *ea, unsigned int strong)
    cdef void yasm_ea_set_segreg(yasm_effaddr *ea, size_t segreg,
            size_t line)
    cdef void yasm_ea_destroy(yasm_effaddr *ea)
    cdef void yasm_ea_print(yasm_effaddr *ea, FILE *f, int indent_level)

    cdef void yasm_bc_set_multiple(yasm_bytecode *bc, yasm_expr *e)
    cdef yasm_bytecode* yasm_bc_create_data(yasm_datavalhead *datahead,
            unsigned int size, int append_zero, size_t line)
    cdef yasm_bytecode* yasm_bc_create_leb128(yasm_datavalhead *datahead,
            int sign, size_t line)
    cdef yasm_bytecode* yasm_bc_create_reserve(yasm_expr *numitems,
            unsigned int itemsize, size_t line)
    cdef yasm_bytecode* yasm_bc_create_incbin(char *filename,
            yasm_expr *start, yasm_expr *maxlen, size_t line)
    cdef yasm_bytecode* yasm_bc_create_align(yasm_expr *boundary,
            yasm_expr *fill, yasm_expr *maxskip,
            unsigned char **code_fill, size_t line)
    cdef yasm_bytecode* yasm_bc_create_org(size_t start,
            size_t line)
    cdef yasm_bytecode* yasm_bc_create_insn(yasm_arch *arch,
            size_t insn_data[4], int num_operands,
            yasm_insn_operands *operands, size_t line)
    cdef yasm_bytecode* yasm_bc_create_empty_insn(yasm_arch *arch,
            size_t insn_data[4], int num_operands,
            yasm_insn_operands *operands, size_t line)
    cdef void yasm_bc_insn_add_prefix(yasm_bytecode *bc,
            size_t prefix_data[4])
    cdef void yasm_bc_insn_add_seg_prefix(yasm_bytecode *bc,
            size_t segreg)
    cdef yasm_section* yasm_bc_get_section(yasm_bytecode *bc)
    cdef void yasm_bc_destroy(yasm_bytecode *bc)
    cdef void yasm_bc_print(yasm_bytecode *bc, FILE *f, int indent_level)
    cdef void yasm_bc_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc)
    cdef yasm_intnum *yasm_common_calc_bc_dist(yasm_bytecode *precbc1,
            yasm_bytecode *precbc2)
    cdef yasm_bc_resolve_flags yasm_bc_resolve(yasm_bytecode *bc, int save,
            yasm_calc_bc_dist_func calc_bc_dist)
    cdef unsigned char* yasm_bc_tobytes(yasm_bytecode *bc,
            unsigned char *buf, size_t *bufsize,
            size_t *multiple, int *gap, void *d,
            yasm_output_expr_func output_expr,
            yasm_output_reloc_func output_reloc)

    cdef yasm_dataval* yasm_dv_create_expr(yasm_expr *expn)
    cdef yasm_dataval* yasm_dv_create_string(char *contents, size_t len)

    cdef void yasm_dvs_initialize(yasm_datavalhead *headp)
    cdef void yasm_dvs_destroy(yasm_datavalhead *headp)
    cdef yasm_dataval* yasm_dvs_append(yasm_datavalhead *headp,
            yasm_dataval *dv)
    cdef void yasm_dvs_print(yasm_datavalhead *headp, FILE *f,
            int indent_level)

    # libyasm/intnum.h
    cdef void BitVector_Boot()
    cdef void BitVector_Shutdown()
    cdef void yasm_intnum_initialize()
    cdef void yasm_intnum_cleanup()
    cdef yasm_intnum* yasm_intnum_create_dec(char *str, size_t line)
    cdef yasm_intnum* yasm_intnum_create_bin(char *str, size_t line)
    cdef yasm_intnum* yasm_intnum_create_oct(char *str, size_t line)
    cdef yasm_intnum* yasm_intnum_create_hex(char *str, size_t line)
    cdef yasm_intnum* yasm_intnum_create_charconst_nasm(char *str, size_t line)
    cdef yasm_intnum* yasm_intnum_create_uint(unsigned int i)
    cdef yasm_intnum* yasm_intnum_create_int(int i)
    cdef yasm_intnum* yasm_intnum_copy(yasm_intnum *intn)
    cdef void yasm_intnum_destroy(yasm_intnum *intn)
    cdef void yasm_intnum_calc(yasm_intnum *acc, yasm_expr_op op,
            yasm_intnum *operand, size_t line)
    cdef void yasm_intnum_zero(yasm_intnum *intn)
    cdef void yasm_intnum_set_uint(yasm_intnum *intn, unsigned int val)
    cdef int yasm_intnum_is_zero(yasm_intnum *intn)
    cdef int yasm_intnum_is_pos1(yasm_intnum *intn)
    cdef int yasm_intnum_is_neg1(yasm_intnum *intn)
    cdef int yasm_intnum_sign(yasm_intnum *intn)
    cdef unsigned int yasm_intnum_get_uint(yasm_intnum *intn)
    cdef int yasm_intnum_get_int(yasm_intnum *intn)
    cdef void yasm_intnum_get_sized(yasm_intnum *intn, unsigned char *ptr,
            size_t destsize, size_t valsize, int shift, int bigendian,
            int warn, size_t line)
    cdef int yasm_intnum_check_size(yasm_intnum *intn, size_t size,
            size_t rshift, int rangetype)
    cdef size_t yasm_intnum_get_leb128(yasm_intnum *intn,
            unsigned char *ptr, int sign)
    cdef size_t yasm_intnum_size_leb128(yasm_intnum *intn, int sign)
    cdef size_t yasm_get_sleb128(int v, unsigned char *ptr)
    cdef size_t yasm_size_sleb128(int v)
    cdef size_t yasm_get_uleb128(unsigned int v, unsigned char *ptr)
    cdef size_t yasm_size_uleb128(unsigned int v)
    cdef void yasm_intnum_print(yasm_intnum *intn, FILE *f)

    # libyasm/floatnum.h
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

    # libyasm/symrec.h
    ctypedef int(*yasm_symtab_traverse_callback)(yasm_symrec *sym, void *d)
    ctypedef yasm_bytecode *yasm_symrec_get_label_bytecodep

    cdef yasm_symtab* yasm_symtab_create()
    cdef void yasm_symtab_destroy(yasm_symtab *symtab)
    cdef yasm_symrec* yasm_symtab_use(yasm_symtab *symtab, char *name,
            size_t line)
    cdef yasm_symrec* yasm_symtab_get(yasm_symtab *symtab, char *name)
    cdef yasm_symrec* yasm_symtab_define_equ(yasm_symtab *symtab,
            char *name, yasm_expr *e, size_t line)
    cdef yasm_symrec* yasm_symtab_define_label(yasm_symtab *symtab,
            char *name, yasm_bytecode *precbc, int in_table, size_t line)
    cdef yasm_symrec* yasm_symtab_define_label2(char *name,
            yasm_bytecode *precbc, int in_table, size_t line)
    cdef yasm_symrec* yasm_symtab_define_special(yasm_symtab *symtab,
            char *name, yasm_sym_vis vis)
    cdef yasm_symrec* yasm_symtab_declare(yasm_symtab *symtab,
            char *name, yasm_sym_vis vis, size_t line)
    cdef void yasm_symrec_declare(yasm_symrec *symrec, yasm_sym_vis vis,
            size_t line)
    cdef int yasm_symtab_traverse(yasm_symtab *symtab, void *d,
            yasm_symtab_traverse_callback func)
    cdef void yasm_symtab_parser_finalize(yasm_symtab *symtab,
            int undef_extern, yasm_objfmt *objfmt)
    cdef void yasm_symtab_print(yasm_symtab *symtab, FILE *f, int indent_level)

    cdef char* yasm_symrec_get_name(yasm_symrec *sym)
    cdef yasm_sym_vis yasm_symrec_get_visibility(yasm_symrec *sym)
    cdef yasm_expr* yasm_symrec_get_equ(yasm_symrec *sym)
    cdef int yasm_symrec_get_label(yasm_symrec *sym,
            yasm_symrec_get_label_bytecodep *precbc)
    cdef int yasm_symrec_is_special(yasm_symrec *sym)
    cdef void* yasm_symrec_get_data(yasm_symrec *sym,
            yasm_assoc_data_callback *callback)
    cdef void yasm_symrec_add_data(yasm_symrec *sym,
            yasm_assoc_data_callback *callback, void *data)
    cdef void yasm_symrec_print(yasm_symrec *sym, FILE *f, int indent_level)

cdef class Register
cdef class IntNum
cdef class FloatNum
cdef class Expression
cdef class SymbolTable
cdef class Symbol
cdef class ObjectFormat
cdef class Bytecode

cdef class Register:
    cdef size_t reg
    def __new__(self, reg):
        self.reg = reg

# TODO: rework __new__ / __int__ / __long__ to support > int values
cdef class IntNum:
    cdef yasm_intnum *intn
    def __new__(self, value, base=None):
        self.intn = NULL
        val = None
        
        if isinstance(value, IntNum):
            self.intn = yasm_intnum_copy((<IntNum>value).intn)
            return

        if isinstance(value, str):
            val = int(value, base)
        elif isinstance(value, (int, long)):
            val = value

        if val is None:
            raise ValueError

        if val < 0:
            self.intn = yasm_intnum_create_int(val)
        else:
            self.intn = yasm_intnum_create_uint(val)

    def __dealloc__(self):
        if self.intn != NULL: yasm_intnum_destroy(self.intn)

    def __int__(self):
        if yasm_intnum_sign(self.intn) < 0:
            return yasm_intnum_get_int(self.intn)
        else:
            return yasm_intnum_get_uint(self.intn)

    def __repr__(self):
        return str(int(self))

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

cdef class FloatNum:
    cdef yasm_floatnum *flt
    def __new__(self, value):
        self.flt = NULL
        if isinstance(value, FloatNum):
            self.flt = yasm_floatnum_copy((<FloatNum>value).flt)
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

cdef class Expression:
    cdef yasm_expr *expr
    cdef yasm_expr_op op

    def __new__(self, op, a, b, line, *args, **kw):
        self.expr = NULL
        op = __op.get(op, op)

        self.expr = yasm_expr_create(op,
                self.__new_item(a),
                self.__new_item(b),
                line)

    cdef yasm_expr__item* __new_item(self, value) except NULL:
        if isinstance(value, Expression):
            return yasm_expr_expr((<Expression>value).expr)
        elif isinstance(value, Symbol):
            return yasm_expr_sym((<Symbol>value).sym)
        elif isinstance(value, Register):
            return yasm_expr_reg((<Register>value).reg)
        elif isinstance(value, float):
            return yasm_expr_float((<FloatNum>value).flt)
        elif isinstance(value, IntNum):
            return yasm_expr_int(yasm_intnum_copy((<IntNum>value).intn))
        else:
            try:
                intnum = IntNum(value)
            except:
                raise ValueError("Invalid item value type '%s'" % type(value))
            else:
                return yasm_expr_int(yasm_intnum_copy((<IntNum>intnum).intn))

    def __dealloc__(self):
        if self.expr != NULL: yasm_expr_destroy(self.expr)

cdef class SymbolTable:
    cdef yasm_symtab *symtab
    def __new__(self):
        self.symtab = yasm_symtab_create()

    def __dealloc__(self):
        if self.symtab != NULL: yasm_symtab_destroy(self.symtab)

    def use(self, name, line):
        return self.__make_symbol(name,
            yasm_symtab_use(self.symtab, name, line))

    def define_equ(self, name, expr, line):
        return self.__make_symbol(name,
            yasm_symtab_define_equ(self.symtab, name, (<Expression>expr).expr, line))

    def define_label(self, name, precbc, in_table, line):
        return self.__make_symbol(name,
            yasm_symtab_define_label(self.symtab, name, (<Bytecode>precbc).bc,
                in_table, line))
        #define2 goes on bytecode

    def define_special(self, name, vis):
        return self.__make_symbol(name,
            yasm_symtab_define_special(self.symtab, name, vis))

    def declare(self, name, vis, line):
        return self.__make_symbol(name,
            yasm_symtab_declare(self.symtab, name, vis, line))

    #def traverse(self, func, *args, **kw):
    #    return yasm_symtab_traverse(self.symtab, (args, kw), func)

    def parser_finalize(self, undef_extern, objfmt):
        yasm_symtab_parser_finalize(self.symtab, undef_extern,
                (<ObjectFormat>objfmt).objfmt)

    cdef object __make_symbol(self, name, yasm_symrec *symrec):
        cdef Symbol sym
        sym = Symbol(name)
        sym.sym = symrec
        return sym

cdef class Symbol:
    cdef yasm_symrec *sym

    def __new__(self, name):
        self.sym = NULL

    # no deref or destroy necessary

    def get_name(self): return yasm_symrec_get_name(self.sym)
    def get_visibility(self): return yasm_symrec_get_visibility(self.sym)
    def get_equ(self):
        return self.__make_expression(yasm_symrec_get_equ(self.sym))
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

    cdef object __make_expression(self, yasm_expr *expr):
        cdef Expression e
        e = Expression()
        e.expr = expr
        return e

cdef class Bytecode:
    cdef yasm_bytecode *bc

cdef class ObjectFormat:
    cdef yasm_objfmt *objfmt

#cdef class DataVal:
#    cdef yasm_dataval *dv
#
#    def __new__(self, string_or_expr):
#        if isinstance(string_or_expr, Expression):
#            self.dv = yasm_dv_create_expr(string_or_expr)
#        else:
#            self.dv = yasm_dv_create_string(string_or_expr,
#                    len(string_or_expr))
#
#    #def __dealloc__(self):
#        #yasm_dv_destroy(self.dv)

#######################################
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

import operator
__op = {
    operator.__add__ : YASM_EXPR_ADD,
    operator.add : YASM_EXPR_ADD,
    operator.__and__ : YASM_EXPR_AND,
    operator.and_ : YASM_EXPR_AND,
    operator.__div__ : YASM_EXPR_SIGNDIV,
    operator.__floordiv__: YASM_EXPR_SIGNDIV,
    operator.div : YASM_EXPR_SIGNDIV,
    operator.floordiv: YASM_EXPR_SIGNDIV,
    operator.__ge__: YASM_EXPR_GE,
    operator.ge: YASM_EXPR_GE,
    operator.__gt__: YASM_EXPR_GT,
    operator.gt: YASM_EXPR_GT,
    operator.__inv__: YASM_EXPR_NOT,
    operator.__invert__: YASM_EXPR_NOT,
    operator.inv: YASM_EXPR_NOT,
    operator.invert: YASM_EXPR_NOT,
    operator.__le__: YASM_EXPR_LE,
    operator.le: YASM_EXPR_LE,
    operator.__lt__: YASM_EXPR_LT,
    operator.lt: YASM_EXPR_LT,
    operator.__mod__: YASM_EXPR_SIGNMOD,
    operator.mod: YASM_EXPR_SIGNMOD,
    operator.__mul__: YASM_EXPR_MUL,
    operator.mul: YASM_EXPR_MUL,
    operator.__neg__: YASM_EXPR_NEG,
    operator.neg: YASM_EXPR_NEG,
    operator.__not__: YASM_EXPR_LNOT,
    operator.not_: YASM_EXPR_LNOT,
    operator.__or__: YASM_EXPR_OR,
    operator.or_: YASM_EXPR_OR,
    operator.__sub__: YASM_EXPR_SUB,
    operator.sub: YASM_EXPR_SUB,
    operator.__xor__: YASM_EXPR_XOR,
    operator.xor: YASM_EXPR_XOR,
}
del operator

#######################################
