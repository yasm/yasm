/* $IdPath$
 * Expression handling
 *
 *  Copyright (C) 2001  Michael Urman, Peter Johnson
 *
 *  This file is part of YASM.
 *
 *  YASM is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  YASM is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "util.h"

#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif

#include <libintl.h>
#define _(String)	gettext(String)
#ifdef gettext_noop
#define N_(String)	gettext_noop(String)
#else
#define N_(String)	(String)
#endif

#include "bitvect.h"

#include "globals.h"
#include "errwarn.h"
#include "intnum.h"
#include "floatnum.h"
#include "expr.h"
#include "symrec.h"

RCSID("$IdPath$");

typedef enum {
    EXPR_NONE,			/* for left side of a NOT, NEG, etc. */
    EXPR_SYM,
    EXPR_EXPR,
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_REG
} ExprType;

struct ExprItem {
    ExprType type;
    union {
	symrec *sym;
	expr *expn;
	intnum *intn;
	floatnum *flt;
	struct reg {
	    unsigned char num;
	    unsigned char size;	/* in bits, eg AX=16, EAX=32 */
	} reg;
    } data;
};

struct expr {
    ExprItem left, right;
    ExprOp op;
    char *filename;
    unsigned long line;
};

static int expr_traverse_nodes_post(expr *e, void *d,
				    int (*func) (expr *e, void *d));
static int expr_traverse_leaves_in(expr *e, void *d,
				   int (*func) (ExprItem *ei, void *d));

/* allocate a new expression node, with children as defined.
 * If it's a unary operator, put the element on the right */
expr *
expr_new(ExprItem *left, ExprOp op, ExprItem *right)
{
    expr *ptr;
    ptr = xmalloc(sizeof(expr));

    ptr->left.type = EXPR_NONE;
    ptr->op = op;
    ptr->right.type = EXPR_NONE;
    if (left) {
	memcpy(&ptr->left, left, sizeof(ExprItem));
	free(left);
    }
    if (right) {
	memcpy(&ptr->right, right, sizeof(ExprItem));
	free(right);
    } else {
	InternalError(__LINE__, __FILE__,
		      _("Right side of expression must exist"));
    }

    ptr->filename = xstrdup(in_filename);
    ptr->line = line_number;

    return ptr;
}

/* helpers */
ExprItem *
ExprSym(symrec *s)
{
    ExprItem *e = xmalloc(sizeof(ExprItem));
    e->type = EXPR_SYM;
    e->data.sym = s;
    return e;
}

ExprItem *
ExprExpr(expr *x)
{
    ExprItem *e = xmalloc(sizeof(ExprItem));
    e->type = EXPR_EXPR;
    e->data.expn = x;
    return e;
}

ExprItem *
ExprInt(intnum *i)
{
    ExprItem *e = xmalloc(sizeof(ExprItem));
    e->type = EXPR_INT;
    e->data.intn = i;
    return e;
}

ExprItem *
ExprFloat(floatnum *f)
{
    ExprItem *e = xmalloc(sizeof(ExprItem));
    e->type = EXPR_FLOAT;
    e->data.flt = f;
    return e;
}

ExprItem *
ExprReg(unsigned char reg, unsigned char size)
{
    ExprItem *e = xmalloc(sizeof(ExprItem));
    e->type = EXPR_REG;
    e->data.reg.num = reg;
    e->data.reg.size = size;
    return e;
}

static void
expr_copy_side(ExprItem *dest, const ExprItem *ei)
{
    dest->type = ei->type;
    switch (ei->type) {
	case EXPR_SYM:
	    dest->data.sym = ei->data.sym;
	    break;
	case EXPR_EXPR:
	    dest->data.expn = expr_copy(ei->data.expn);
	    break;
	case EXPR_INT:
	    dest->data.intn = intnum_copy(ei->data.intn);
	    break;
	case EXPR_FLOAT:
	    dest->data.flt = floatnum_copy(ei->data.flt);
	    break;
	case EXPR_REG:
	    dest->data.reg.num = ei->data.reg.num;
	    dest->data.reg.size = ei->data.reg.size;
	    break;
	default:
	    break;
    }
}

expr *
expr_copy(const expr *e)
{
    expr *n;
    
    if (!e)
	return 0;

    n = xmalloc(sizeof(expr));

    expr_copy_side(&n->left, &e->left);
    expr_copy_side(&n->right, &e->right);
    n->op = e->op;
    n->filename = xstrdup(e->filename);
    n->line = e->line;

    return n;
}

static
int expr_delete_each(expr *e, void *d)
{
    switch (e->left.type) {
	case EXPR_INT:
	    intnum_delete(e->left.data.intn);
	    break;
	case EXPR_FLOAT:
	    floatnum_delete(e->left.data.flt);
	    break;
	default:
	    break;  /* none of the other types needs to be deleted */
    }
    switch (e->right.type) {
	case EXPR_INT:
	    intnum_delete(e->right.data.intn);
	    break;
	case EXPR_FLOAT:
	    floatnum_delete(e->right.data.flt);
	    break;
	default:
	    break;  /* none of the other types needs to be deleted */
    }
    free(e->filename);
    free(e);	/* free ourselves */
    return 0;	/* don't stop recursion */
}

void
expr_delete(expr *e)
{
    expr_traverse_nodes_post(e, NULL, expr_delete_each);
}

static int
expr_contains_float_callback(ExprItem *ei, void *d)
{
    return (ei->type == EXPR_FLOAT);
}

int
expr_contains_float(expr *e)
{
    return expr_traverse_leaves_in(e, NULL, expr_contains_float_callback);
}

typedef struct checkea_invalid16_data {
    enum havereg {
	HAVE_NONE = 0,
	HAVE_BX = 1 << 0,
	HAVE_SI = 1 << 1,
	HAVE_DI = 1 << 2,
	HAVE_BP = 1 << 3
    } havereg;
    int regleft, regright;
} checkea_invalid16_data;

/* Only works if ei->type == EXPR_REG (doesn't check).
 * Overwrites ei with intnum of 0 (to eliminate regs from the final expr).
 */
static int
expr_checkea_invalid16_reg(ExprItem *ei, checkea_invalid16_data *data)
{
    /* in order: ax,cx,dx,bx,sp,bp,si,di */
    static const char reg16[8] = {0,0,0,HAVE_BX,0,HAVE_BP,HAVE_SI,HAVE_DI};

    /* don't allow 32-bit registers */
    if (ei->data.reg.size != 16)
	return 1;

    /* only allow BX, SI, DI, BP */
    if (!reg16[ei->data.reg.num & 7])	/* & 7 is sanity check */
	return 1;
    /* OR it into havereg mask */
    data->havereg |= reg16[ei->data.reg.num & 7];

    /* only one of each of BX/BP, SI/DI pairs is legal */
    if ((data->havereg & HAVE_BX) && (data->havereg & HAVE_BP))
	return 1;
    if ((data->havereg & HAVE_SI) && (data->havereg & HAVE_DI))
	return 1;

    /* overwrite with 0 to eliminate register from displacement expr */
    ei->type = EXPR_INT;
    ei->data.intn = intnum_new_int(0);

    /* we're okay */
    return 0;
}

/* Returns 0 if expression is correct up to this point, 1 if there's an error.
 * Updates d with new info if necessary.
 * Must be called using expr_traverse_nodes_post() to work properly.
 */
static int
expr_checkea_invalid16_callback(expr *e, void *d)
{
    checkea_invalid16_data *data = (checkea_invalid16_data *)d;

    switch (e->left.type) {
	case EXPR_FLOAT:
	    return 1;	    /* disallow float values */
	case EXPR_REG:
	    /* record and check register values */
	    if (expr_checkea_invalid16_reg(&e->left, data))
		return 1;
	    data->regleft = 1;
	    break;
	default:
	    break;
    }
    switch (e->right.type) {
	case EXPR_FLOAT:
	    return 1;	    /* disallow float values */
	case EXPR_REG:
	    /* record and check register values */
	    if (expr_checkea_invalid16_reg(&e->right, data))
		return 1;
	    data->regright = 1;
	    break;
	default:
	    break;
    }

    /* only op allowed with register on right is ADD (and of course, IDENT) */
    if (data->regright && e->op != EXPR_ADD && e->op != EXPR_IDENT)
	return 1;

    /* only ops allowed with register on left are ADD or SUB */
    if ((data->regleft && !data->regright) && e->op != EXPR_ADD &&
	e->op != EXPR_SUB)
	return 1;

    /* we're okay */
    return 0;
}

static int
expr_checkea_getregsize_callback(ExprItem *ei, void *d)
{
    unsigned char *addrsize = (unsigned char *)d;

    if (ei->type == EXPR_REG) {
	*addrsize = ei->data.reg.size;
	return 1;
    } else
	return 0;
}

int
expr_checkea(expr **e, unsigned char *addrsize, unsigned char bits,
	     unsigned char *displen, unsigned char *modrm,
	     unsigned char *v_modrm, unsigned char *n_modrm,
	     unsigned char *sib, unsigned char *v_sib, unsigned char *n_sib)
{
    const intnum *intn;
    long dispval;

    if (*addrsize == 0) {
	/* we need to figure out the address size from what we know about:
	 * - the displacement length
	 * - what registers are used in the expression
	 * - the bits setting
	 */
	switch (*displen) {
	    case 4:
		/* must be 32-bit */
		*addrsize = 32;
		break;
	    case 2:
		/* must be 16-bit */
		*addrsize = 16;
		break;
	    default:
		/* check for use of 16 or 32-bit registers; if none are used
		 * default to bits setting.
		 */
		if (!expr_traverse_leaves_in(*e, addrsize,
					     expr_checkea_getregsize_callback))
		    *addrsize = bits;
	}
    }

    if (*addrsize == 32 && (*n_modrm || *n_sib)) {
	/* TODO */
    } else if (*addrsize == 16 && *n_modrm) {
	static const unsigned char modrm16[16] = {
	    0006 /* disp16  */, 0007 /* [BX]    */, 0004 /* [SI]    */,
	    0000 /* [BX+SI] */, 0005 /* [DI]    */, 0001 /* [BX+DI] */,
	    0377 /* invalid */, 0377 /* invalid */, 0006 /* [BP]+d  */,
	    0377 /* invalid */, 0002 /* [BP+SI] */, 0377 /* invalid */,
	    0003 /* [BP+DI] */, 0377 /* invalid */, 0377 /* invalid */,
	    0377 /* invalid */
	};
	checkea_invalid16_data data;

	data.havereg = HAVE_NONE;
	data.regleft = 0;
	data.regright = 0;

	/* 16-bit cannot have SIB */
	*sib = 0;
	*v_sib = 0;
	*n_sib = 0;

	/* Check for valid effective address, and get used registers */
	if (expr_traverse_nodes_post(*e, &data,
				     expr_checkea_invalid16_callback)) {
	    ErrorAt((*e)->filename, (*e)->line, _("invalid effective address"));
	    return 0;
	}

	/* Simplify expr, which is now really just the displacement. This
	 * should get rid of the 0's we put in for registers in the callback.
	 */
	expr_simplify(*e);

	/* sanity check the modrm value; shouldn't be invalid because we
	 * checked for that in the callback!
	 */
	if (modrm16[data.havereg] & 0070)
	    InternalError(__LINE__, __FILE__, _("invalid havereg value"));

	*modrm |= modrm16[data.havereg];

	*v_modrm = 0;	/* default to not yet valid */

	switch (*displen) {
	    case 0:
		/* the displacement length hasn't been forced, try to
		 * determine what it is.
		 */
		switch (data.havereg) {
		    case HAVE_NONE:
			/* no register in expression, so it must be disp16, and
			 * as the Mod bits are set to 0 above, we're done with
			 * the ModRM byte.
			 */
			*displen = 2;
			*v_modrm = 1;
			break;
		    case HAVE_BP:
			/* for BP, there *must* be a displacement value, but we
			 * may not know the size (8 or 16) for sure right now.
			 * We can't leave displen at 0, because that just means
			 * unknown displacement, including none.
			 */
			*displen = 0xff;
			break;
		    default:
			break;
		}

		intn = expr_get_intnum(*e);
		if (!intn)
		    break;	    /* expr still has unknown values */

		/* make sure the displacement will fit in 16 bits if unsigned,
		 * and 8 bits if signed.
		 */
		if (!intnum_check_size(intn, 2, 0) &&
		    !intnum_check_size(intn, 1, 1)) {
		    ErrorAt((*e)->filename, (*e)->line,
			    _("invalid effective address"));
		    return 0;
		}

		/* don't try to find out what size displacement we have if
		 * displen is known.
		 */
		if (*displen != 0 && *displen != 0xff)
		    break;

		/* Don't worry about overflows here (it's already guaranteed
		 * to be 16 or 8 bits).
		 */
		dispval = intnum_get_int(intn);

		/* Figure out what size displacement we will have. */
		if (*displen != 0xff && dispval == 0) {
		    /* if we know that the displacement is 0 right now,
		     * go ahead and delete the expr (making it so no
		     * displacement value is included in the output).
		     * The Mod bits of ModRM are set to 0 above, and
		     * we're done with the ModRM byte!
		     *
		     * Don't do this if we came from HAVE_BP above, so
		     * check *displen.
		     */
		    expr_delete(*e);
		    *e = (expr *)NULL;
		} else if (dispval >= -128 && dispval <= 127) {
		    /* It fits into a signed byte */
		    *displen = 1;
		    *modrm |= 0100;
		} else {
		    /* It's a 16-bit displacement */
		    *displen = 2;
		    *modrm |= 0200;
		}
		*v_modrm = 1;	/* We're done with ModRM */

		break;

	    /* If not 0, the displacement length was forced; set the Mod bits
	     * appropriately and we're done with the ModRM byte.  We assume
	     * that the user knows what they're doing if they do an explicit
	     * override, so we don't check for overflow (we'll just truncate
	     * when we output).
	     */
	    case 1:
		*modrm |= 0100;
		*v_modrm = 1;
		break;
	    case 2:
		*modrm |= 0200;
		*v_modrm = 1;
		break;
	    default:
		/* any other size is an error */
		ErrorAt((*e)->filename, (*e)->line,
			_("invalid effective address (displacement size)"));
		return 0;
	}
    }
    return 1;
}

static int
expr_expand_equ_callback(ExprItem *ei, void *d)
{
    const expr *equ_expr;
    if (ei->type == EXPR_SYM) {
	equ_expr = symrec_get_equ(ei->data.sym);
	if (equ_expr) {
	    ei->type = EXPR_EXPR;
	    ei->data.expn = expr_copy(equ_expr);
	}
    }
    return 0;
}

void
expr_expand_equ(expr *e)
{
    expr_traverse_leaves_in(e, NULL, expr_expand_equ_callback);
}

/* Traverse over expression tree, calling func for each operation AFTER the
 * two branches (if expressions) have been traversed (eg, postorder
 * traversal).  The data pointer d is passed to each func call.
 *
 * Stops early (and returns 1) if func returns 1.  Otherwise returns 0.
 */
static int
expr_traverse_nodes_post(expr *e, void *d, int (*func) (expr *e, void *d))
{
    if (!e)
	return 0;

    /* traverse left side */
    if (e->left.type == EXPR_EXPR &&
	expr_traverse_nodes_post(e->left.data.expn, d, func))
	return 1;

    /* traverse right side */
    if (e->right.type == EXPR_EXPR &&
	expr_traverse_nodes_post(e->right.data.expn, d, func))
	return 1;

    /* do callback */
    return func(e, d);
}

/* Traverse over expression tree in order, calling func for each leaf
 * (non-operation).  The data pointer d is passed to each func call.
 *
 * Stops early (and returns 1) if func returns 1.  Otherwise returns 0.
 */
static int
expr_traverse_leaves_in(expr *e, void *d,
			int (*func) (ExprItem *ei, void *d))
{
    if (!e)
	return 0;

    if (e->left.type == EXPR_EXPR) {
	if (expr_traverse_leaves_in(e->left.data.expn, d, func))
	    return 1;
    } else {
	if (func(&e->left, d))
	    return 1;
    }

    if (e->right.type == EXPR_EXPR) {
	if (expr_traverse_leaves_in(e->right.data.expn, d, func))
	    return 1;
    } else
	return func(&e->right, d);

    return 0;
}

/* get rid of unnecessary branches if possible.  report. */
int
expr_simplify(expr *e)
{
    int simplified = 0;
    ExprItem tmp;

    /* try to simplify the left side */
    if (e->left.type == EXPR_EXPR) {
	/* if the left subexpr isn't an IDENT, recurse simplification */
	if (e->left.data.expn->op != EXPR_IDENT)
	    simplified |= expr_simplify(e->left.data.expn);

	/* if the left subexpr is just an IDENT (or string thereof),
	 * pull it up into the current node */
	while (e->left.type == EXPR_EXPR &&
	       e->left.data.expn->op == EXPR_IDENT) {
	    memcpy(&tmp, &(e->left.data.expn->right), sizeof(ExprItem));
	    free(e->left.data.expn);
	    memcpy(&e->left, &tmp, sizeof(ExprItem));
	    simplified = 1;
	}
    }
#if 0
    else if (e->left.type == EXPR_SYM) {
	/* try to get value of symbol */
	if (symrec_get_int_value(e->left.data.sym, &int_val, 0)) {
	    e->left.type = EXPR_INT;
	    /* don't try to free the symrec here. */
	    e->left.data.int_val = int_val;
	    simplified = 1;
	}
    }
#endif

    /* ditto on the right */
    if (e->right.type == EXPR_EXPR) {
	if (e->right.data.expn->op != EXPR_IDENT)
	    simplified |= expr_simplify(e->right.data.expn);

	while (e->right.type == EXPR_EXPR &&
	       e->right.data.expn->op == EXPR_IDENT) {
	    memcpy(&tmp, &(e->right.data.expn->right), sizeof(ExprItem));
	    free(e->right.data.expn);
	    memcpy(&e->right, &tmp, sizeof(ExprItem));
	    simplified = 1;
	}
    }
#if 0
    else if (e->right.type == EXPR_SYM) {
	if (symrec_get_int_value(e->right.data.sym, &int_val, 0)) {
	    e->right.type = EXPR_INT;
	    /* don't try to free the symrec here. */
	    e->right.data.int_val = int_val;
	    simplified = 1;
	}
    }
#endif

    /* catch simple identities like 0+x, 1*x, etc., for x not a num */
    if (e->left.type == EXPR_INT &&
	     ((intnum_is_pos1(e->left.data.intn) && e->op == EXPR_MUL) ||
	      (intnum_is_zero(e->left.data.intn) && e->op == EXPR_ADD) ||
	      (intnum_is_neg1(e->left.data.intn) && e->op == EXPR_AND) ||
	      (intnum_is_zero(e->left.data.intn) && e->op == EXPR_OR))) {
	intnum_delete(e->left.data.intn);
	e->op = EXPR_IDENT;
	simplified = 1;
    }
    /* and the corresponding x+|-0, x*&/1 */
    else if (e->right.type == EXPR_INT &&
	     ((intnum_is_pos1(e->right.data.intn) && e->op == EXPR_MUL) ||
	      (intnum_is_pos1(e->right.data.intn) && e->op == EXPR_DIV) ||
	      (intnum_is_zero(e->right.data.intn) && e->op == EXPR_ADD) ||
	      (intnum_is_zero(e->right.data.intn) && e->op == EXPR_SUB) ||
	      (intnum_is_neg1(e->right.data.intn) && e->op == EXPR_AND) ||
	      (intnum_is_zero(e->right.data.intn) && e->op == EXPR_OR) ||
	      (intnum_is_zero(e->right.data.intn) && e->op == EXPR_SHL) ||
	      (intnum_is_zero(e->right.data.intn) && e->op == EXPR_SHR))) {
	intnum_delete(e->right.data.intn);
	e->op = EXPR_IDENT;
	e->right.type = e->left.type;
	memcpy(&e->right, &e->left, sizeof(ExprItem));
	simplified = 1;
    } else if ((e->left.type == EXPR_INT || e->left.type == EXPR_NONE) &&
	e->right.type == EXPR_INT && e->op != EXPR_IDENT) {
	intnum_calc(e->left.data.intn, e->op, e->right.data.intn);
	intnum_delete(e->right.data.intn);
	e->right.data.intn = e->left.data.intn;
	e->op = EXPR_IDENT;
	simplified = 1;
    }

    return simplified;
}

const intnum *
expr_get_intnum(expr *e)
{
    while (!(e->op == EXPR_IDENT && e->right.type == EXPR_INT) &&
	   expr_simplify(e))
	;

    if (e->op == EXPR_IDENT && e->right.type == EXPR_INT)
	return e->right.data.intn;
    else
	return (intnum *)NULL;
}

void
expr_print(expr *e)
{
    static const char *regs[] = {"ax","cx","dx","bx","sp","bp","si","di"};

    if (e->op != EXPR_IDENT) {
	switch (e->left.type) {
	    case EXPR_SYM:
		printf("%s", symrec_get_name(e->left.data.sym));
		break;
	    case EXPR_EXPR:
		printf("(");
		expr_print(e->left.data.expn);
		printf(")");
		break;
	    case EXPR_INT:
		intnum_print(e->left.data.intn);
		break;
	    case EXPR_FLOAT:
		floatnum_print(e->left.data.flt);
		break;
	    case EXPR_REG:
		if (e->left.data.reg.size == 32)
		    printf("e");
		printf("%s", regs[e->left.data.reg.num]);
		break;
	    case EXPR_NONE:
		break;
	}
    }
    switch (e->op) {
	case EXPR_ADD:
	    printf("+");
	    break;
	case EXPR_SUB:
	    printf("-");
	    break;
	case EXPR_MUL:
	    printf("*");
	    break;
	case EXPR_DIV:
	    printf("/");
	    break;
	case EXPR_SIGNDIV:
	    printf("//");
	    break;
	case EXPR_MOD:
	    printf("%%");
	    break;
	case EXPR_SIGNMOD:
	    printf("%%%%");
	    break;
	case EXPR_NEG:
	    printf("-");
	    break;
	case EXPR_NOT:
	    printf("~");
	    break;
	case EXPR_OR:
	    printf("|");
	    break;
	case EXPR_AND:
	    printf("&");
	    break;
	case EXPR_XOR:
	    printf("^");
	    break;
	case EXPR_SHL:
	    printf("<<");
	    break;
	case EXPR_SHR:
	    printf(">>");
	    break;
	case EXPR_LOR:
	    printf("||");
	    break;
	case EXPR_LAND:
	    printf("&&");
	    break;
	case EXPR_LNOT:
	    printf("!");
	    break;
	case EXPR_LT:
	    printf("<");
	    break;
	case EXPR_GT:
	    printf(">");
	    break;
	case EXPR_LE:
	    printf("<=");
	    break;
	case EXPR_GE:
	    printf(">=");
	    break;
	case EXPR_NE:
	    printf("!=");
	    break;
	case EXPR_EQ:
	    printf("==");
	    break;
	case EXPR_IDENT:
	    break;
    }
    switch (e->right.type) {
	case EXPR_SYM:
	    printf("%s", symrec_get_name(e->right.data.sym));
	    break;
	case EXPR_EXPR:
	    printf("(");
	    expr_print(e->right.data.expn);
	    printf(")");
	    break;
	case EXPR_INT:
	    intnum_print(e->right.data.intn);
	    break;
	case EXPR_FLOAT:
	    floatnum_print(e->right.data.flt);
	    break;
	case EXPR_REG:
	    if (e->right.data.reg.size == 32)
		printf("e");
	    printf("%s", regs[e->right.data.reg.num]);
	    break;
	case EXPR_NONE:
	    break;
    }
}
