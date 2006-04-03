cdef extern from "libyasm/expr.h":
    cdef struct yasm_expr__item

    cdef yasm_expr *yasm_expr_create(yasm_expr_op op, yasm_expr__item *a,
            yasm_expr__item *b, unsigned long line)
    cdef yasm_expr__item *yasm_expr_sym(yasm_symrec *sym)
    cdef yasm_expr__item *yasm_expr_expr(yasm_expr *e)
    cdef yasm_expr__item *yasm_expr_int(yasm_intnum *intn)
    cdef yasm_expr__item *yasm_expr_float(yasm_floatnum *flt)
    cdef yasm_expr__item *yasm_expr_reg(unsigned long reg)
    cdef yasm_expr *yasm_expr_create_tree(yasm_expr *l, yasm_expr_op op,
            yasm_expr *r, unsigned long line)
    cdef yasm_expr *yasm_expr_create_branch(yasm_expr_op op,
            yasm_expr *r, unsigned long line)
    cdef yasm_expr *yasm_expr_create_ident(yasm_expr *r, unsigned long line)
    cdef yasm_expr *yasm_expr_copy(yasm_expr *e)
    cdef void yasm_expr_destroy(yasm_expr *e)
    cdef int yasm_expr_is_op(yasm_expr *e, yasm_expr_op op)
    ctypedef yasm_expr * (*yasm_expr_xform_func) (yasm_expr *e, void *d)

    cdef struct yasm__exprhead
    cdef yasm_expr *yasm_expr__level_tree(yasm_expr *e, int fold_const,
            int simplify_ident, int simplify_reg_mul,
            yasm_calc_bc_dist_func calc_bc_dist,
            yasm_expr_xform_func expr_xform_extra,
            void *expr_xform_extra_data, yasm__exprhead *eh, int *error)
    cdef yasm_expr *yasm_expr_simplify(yasm_expr *e, yasm_calc_bc_dist_func cbd)
    cdef yasm_expr *yasm_expr_extract_segoff(yasm_expr **ep)
    cdef yasm_expr *yasm_expr_extract_wrt(yasm_expr **ep)
    cdef yasm_intnum *yasm_expr_get_intnum(yasm_expr **ep,
            yasm_calc_bc_dist_func calc_bc_dist)
    cdef yasm_symrec *yasm_expr_get_symrec(yasm_expr **ep, int simplify)
    cdef unsigned long *yasm_expr_get_reg(yasm_expr **ep, int simplify)
    cdef void yasm_expr_print(yasm_expr *e, FILE *f)

cdef extern from "libyasm/expr-int.h":
    cdef enum yasm_expr__type:
        YASM_EXPR_NONE
        YASM_EXPR_REG
        YASM_EXPR_INT
        YASM_EXPR_FLOAT
        YASM_EXPR_SYM
        YASM_EXPR_SYMEXP
        YASM_EXPR_EXPR

    cdef union yasm_expr__type_data:
        yasm_symrec *sym
        yasm_expr *expn
        yasm_intnum *intn
        yasm_floatnum *flt
        unsigned long reg

    cdef struct yasm_expr__item:
        yasm_expr__type type
        yasm_expr__type_data data

    cdef struct yasm_expr:
        yasm_expr_op op
        unsigned long line
        int numterms
        yasm_expr__item terms[2]

    cdef int yasm_expr__traverse_leaves_in_const(yasm_expr *e,
            void *d, int (*func) (yasm_expr__item *ei, void *d))
    cdef int yasm_expr__traverse_leaves_in(yasm_expr *e, void *d,
            int (*func) (yasm_expr__item *ei, void *d))

    cdef void yasm_expr__order_terms(yasm_expr *e)

    cdef yasm_expr *yasm_expr__copy_except(yasm_expr *e, int excpt)

    cdef int yasm_expr__contains(yasm_expr *e, yasm_expr__type t)

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

cdef class Expression
cdef object __make_expression(yasm_expr *expr):
    return Expression(PyCObject_FromVoidPtr(expr, NULL))

cdef class Expression:
    cdef yasm_expr *expr

    def __new__(self, op, *args, **kwargs):
        self.expr = NULL

        if isinstance(op, Expression):
            self.expr = yasm_expr_copy((<Expression>op).expr)
            return
        if PyCObject_Check(op):  # should check Desc
            self.expr = <yasm_expr *>PyCObject_AsVoidPtr(op)
            return

        cdef size_t numargs
        cdef unsigned long line

        op = __op.get(op, op)
        numargs = len(args)
        line = kwargs.get('line', 0)

        if numargs == 0 or numargs > 2:
            raise NotImplementedError
        elif numargs == 2:
            self.expr = yasm_expr_create(op, self.__new_item(args[0]),
                                         self.__new_item(args[1]), line)
        else:
            self.expr = yasm_expr_create(op, self.__new_item(args[0]), NULL,
                                         line)

    cdef yasm_expr__item* __new_item(self, value) except NULL:
        cdef yasm_expr__item *retval
        if isinstance(value, Expression):
            return yasm_expr_expr(yasm_expr_copy((<Expression>value).expr))
        #elif isinstance(value, Symbol):
        #    return yasm_expr_sym((<Symbol>value).sym)
        #elif isinstance(value, Register):
        #    return yasm_expr_reg((<Register>value).reg)
        elif isinstance(value, FloatNum):
            return yasm_expr_float(yasm_floatnum_copy((<FloatNum>value).flt))
        elif isinstance(value, IntNum):
            return yasm_expr_int(yasm_intnum_copy((<IntNum>value).intn))
        else:
            try:
                intnum = IntNum(value)
            except:
                raise ValueError("Invalid item value type '%s'" % type(value))
            else:
                retval = yasm_expr_int((<IntNum>intnum).intn)
                (<IntNum>intnum).intn = NULL
                return retval

    def __dealloc__(self):
        if self.expr != NULL: yasm_expr_destroy(self.expr)

    def simplify(self):
        self.expr = yasm_expr_simplify(self.expr, NULL) # TODO: cbd

    def extract_segoff(self):
        cdef yasm_expr *retval
        retval = yasm_expr_extract_segoff(&self.expr)
        if retval == NULL:
            raise ValueError("not a SEG:OFF expression")
        return __make_expression(retval)

    def extract_wrt(self):
        cdef yasm_expr *retval
        retval = yasm_expr_extract_wrt(&self.expr)
        if retval == NULL:
            raise ValueError("not a WRT expression")
        return __make_expression(retval)

    def get_intnum(self):
        cdef yasm_intnum *retval
        retval = yasm_expr_get_intnum(&self.expr, NULL) # TODO: cbd
        if retval == NULL:
            raise ValueError("not an intnum expression")
        return __make_intnum(yasm_intnum_copy(retval))

