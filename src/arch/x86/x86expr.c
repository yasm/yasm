/*
 * x86 expression handling
 *
 *  Copyright (C) 2001  Peter Johnson
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
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "bitvect.h"

#include "errwarn.h"
#include "intnum.h"
#include "floatnum.h"
#include "expr.h"

#include "bytecode.h"
#include "arch.h"

#include "x86arch.h"

#include "expr-int.h"


/* Only works if ei->type == EXPR_REG (doesn't check).
 * Overwrites ei with intnum of 0 (to eliminate regs from the final expr).
 */
static /*@null@*/ /*@dependent@*/ int *
x86_expr_checkea_get_reg32(ExprItem *ei, /*returned*/ void *d)
{
    int *data = d;
    int *ret;

    /* don't allow 16-bit registers */
    if ((ei->data.reg & ~7) != X86_REG32)
	return 0;

    ret = &data[ei->data.reg & 7];

    /* overwrite with 0 to eliminate register from displacement expr */
    ei->type = EXPR_INT;
    ei->data.intn = intnum_new_uint(0);

    /* we're okay */
    return ret;
}

typedef struct x86_checkea_reg16_data {
    int bx, si, di, bp;		/* total multiplier for each reg */
} x86_checkea_reg16_data;

/* Only works if ei->type == EXPR_REG (doesn't check).
 * Overwrites ei with intnum of 0 (to eliminate regs from the final expr).
 */
static /*@null@*/ int *
x86_expr_checkea_get_reg16(ExprItem *ei, void *d)
{
    x86_checkea_reg16_data *data = d;
    /* in order: ax,cx,dx,bx,sp,bp,si,di */
    /*@-nullassign@*/
    static int *reg16[8] = {0,0,0,0,0,0,0,0};
    /*@=nullassign@*/
    int *ret;

    reg16[3] = &data->bx;
    reg16[5] = &data->bp;
    reg16[6] = &data->si;
    reg16[7] = &data->di;

    /* don't allow 32-bit registers */
    if ((ei->data.reg & ~7) != X86_REG16)
	return 0;

    /* & 7 for sanity check */
    ret = reg16[ei->data.reg & 7];

    /* only allow BX, SI, DI, BP */
    if (!ret)
	return 0;

    /* overwrite with 0 to eliminate register from displacement expr */
    ei->type = EXPR_INT;
    ei->data.intn = intnum_new_uint(0);

    /* we're okay */
    return ret;
}

/* Distribute over registers to help bring them to the topmost level of e.
 * Also check for illegal operations against registers.
 * Returns 0 if something was illegal, 1 if legal and nothing in e changed,
 * and 2 if legal and e needs to be simplified.
 *
 * Only half joking: Someday make this/checkea able to accept crazy things
 *  like: (bx+di)*(bx+di)-bx*bx-2*bx*di-di*di+di?  Probably not: NASM never
 *  accepted such things, and it's doubtful such an expn is valid anyway
 *  (even though the above one is).  But even macros would be hard-pressed
 *  to generate something like this.
 *
 * e must already have been simplified for this function to work properly
 * (as it doesn't think things like SUB are valid).
 *
 * IMPLEMENTATION NOTE: About the only thing this function really needs to
 * "distribute" is: (non-float-expn or intnum) * (sum expn of registers).
 *
 * TODO: Clean up this code, make it easier to understand.
 */
static int
x86_expr_checkea_distcheck_reg(expr **ep)
{
    expr *e = *ep;
    int i;
    int havereg = -1, havereg_expr = -1;
    int retval = 1;	/* default to legal, no changes */

    for (i=0; i<e->numterms; i++) {
	switch (e->terms[i].type) {
	    case EXPR_REG:
		/* Check op to make sure it's valid to use w/register. */
		if (e->op != EXPR_ADD && e->op != EXPR_MUL &&
		    e->op != EXPR_IDENT)
		    return 0;
		/* Check for reg*reg */
		if (e->op == EXPR_MUL && havereg != -1)
		    return 0;
		havereg = i;
		break;
	    case EXPR_FLOAT:
		/* Floats not allowed. */
		return 0;
	    case EXPR_EXPR:
		if (expr_contains(e->terms[i].data.expn, EXPR_REG)) {
		    int ret2;

		    /* Check op to make sure it's valid to use w/register. */
		    if (e->op != EXPR_ADD && e->op != EXPR_MUL)
			return 0;
		    /* Check for reg*reg */
		    if (e->op == EXPR_MUL && havereg != -1)
			return 0;
		    havereg = i;
		    havereg_expr = i;
		    /* Recurse to check lower levels */
		    ret2 =
			x86_expr_checkea_distcheck_reg(&e->terms[i].data.expn);
		    if (ret2 == 0)
			return 0;
		    if (ret2 == 2)
			retval = 2;
		} else if (expr_contains(e->terms[i].data.expn, EXPR_FLOAT))
		    return 0;	/* Disallow floats */
		break;
	    default:
		break;
	}
    }

    /* just exit if no registers were used */
    if (havereg == -1)
	return retval;

    /* Distribute */
    if (e->op == EXPR_MUL && havereg_expr != -1) {
	expr *ne;

	retval = 2;	/* we're going to change it */

	/* The reg expn *must* be EXPR_ADD at this point.  Sanity check. */
	if (e->terms[havereg_expr].type != EXPR_EXPR ||
	    e->terms[havereg_expr].data.expn->op != EXPR_ADD)
	    InternalError(_("Register expression not ADD or EXPN"));

	/* Iterate over each term in reg expn */
	for (i=0; i<e->terms[havereg_expr].data.expn->numterms; i++) {
	    /* Copy everything EXCEPT havereg_expr term into new expression */
	    ne = expr_copy_except(e, havereg_expr);
	    assert(ne != NULL);
	    /* Copy reg expr term into uncopied (empty) term in new expn */
	    ne->terms[havereg_expr] =
		e->terms[havereg_expr].data.expn->terms[i]; /* struct copy */
	    /* Overwrite old reg expr term with new expn */
	    e->terms[havereg_expr].data.expn->terms[i].type = EXPR_EXPR;
	    e->terms[havereg_expr].data.expn->terms[i].data.expn = ne;
	}

	/* Replace e with expanded reg expn */
	ne = e->terms[havereg_expr].data.expn;
	e->terms[havereg_expr].type = EXPR_NONE;    /* don't delete it! */
	expr_delete(e);				    /* but everything else */
	e = ne;
	/*@-onlytrans@*/
	*ep = ne;
	/*@=onlytrans@*/
    }

    return retval;
}

/* Simplify and determine if expression is superficially valid:
 * Valid expr should be [(int-equiv expn)]+[reg*(int-equiv expn)+...]
 * where the [...] parts are optional.
 *
 * Don't simplify out constant identities if we're looking for an indexreg: we
 * may need the multiplier for determining what the indexreg is!
 *
 * Returns 0 if invalid register usage, 1 if unable to determine all values,
 * and 2 if all values successfully determined and saved in data.
 */
static int
x86_expr_checkea_getregusage(expr **ep, /*@null@*/ int *indexreg, void *data,
			     int *(*get_reg)(ExprItem *ei, void *d),
			     calc_bc_dist_func calc_bc_dist)
{
    int i;
    int *reg;
    expr *e;

    /*@-unqualifiedtrans@*/
    *ep = expr_level_tree(*ep, 1, indexreg == 0, calc_bc_dist, NULL, NULL, NULL);
    /*@=unqualifiedtrans@*/
    assert(*ep != NULL);
    e = *ep;
    switch (x86_expr_checkea_distcheck_reg(ep)) {
	case 0:
	    return 0;
	case 2:
	    /* Need to simplify again */
	    *ep = expr_level_tree(*ep, 1, indexreg == 0, NULL, NULL, NULL, NULL);
	    e = *ep;
	    break;
	default:
	    break;
    }

    switch (e->op) {
	case EXPR_ADD:
	    /* Prescan for non-int multipliers.
	     * This is because if any of the terms is a more complex
	     * expr (eg, undetermined value), we don't want to try to
	     * figure out *any* of the expression, because each register
	     * lookup overwrites the register with a 0 value!  And storing
	     * the state of this routine from one excution to the next
	     * would be a major chore.
	     */
	    for (i=0; i<e->numterms; i++)
		if (e->terms[i].type == EXPR_EXPR) {
		    if (e->terms[i].data.expn->numterms > 2)
			return 1;
		    expr_order_terms(e->terms[i].data.expn);
		    if (e->terms[i].data.expn->terms[1].type != EXPR_INT)
			return 1;
		}

	    /*@fallthrough@*/
	case EXPR_IDENT:
	    /* Check each term for register (and possible multiplier). */
	    for (i=0; i<e->numterms; i++) {
		if (e->terms[i].type == EXPR_REG) {
		    reg = get_reg(&e->terms[i], data);
		    if (!reg)
			return 0;
		    (*reg)++;
		    if (indexreg)
			*indexreg = reg-(int *)data;
		} else if (e->terms[i].type == EXPR_EXPR) {
		    /* Already ordered from ADD above, just grab the value.
		     * Sanity check for EXPR_INT.
		     */
		    if (e->terms[i].data.expn->terms[0].type != EXPR_REG)
			InternalError(_("Register not found in reg expn"));
		    if (e->terms[i].data.expn->terms[1].type != EXPR_INT)
			InternalError(_("Non-integer value in reg expn"));
		    reg = get_reg(&e->terms[i].data.expn->terms[0], data);
		    if (!reg)
			return 0;
		    (*reg) +=
			intnum_get_int(e->terms[i].data.expn->terms[1].data.intn);
		    if (indexreg && *reg > 0)
			*indexreg = reg-(int *)data;
		}
	    }
	    break;
	case EXPR_MUL:
	    /* Here, too, check for non-int multipliers. */
	    if (e->numterms > 2)
		return 1;
	    expr_order_terms(e);
	    if (e->terms[1].type != EXPR_INT)
		return 1;
	    reg = get_reg(&e->terms[0], data);
	    if (!reg)
		return 0;
	    (*reg) += intnum_get_int(e->terms[1].data.intn);
	    if (indexreg)
		*indexreg = reg-(int *)data;
	    break;
	default:
	    /* Should never get here! */
	    break;
    }

    /* Simplify expr, which is now really just the displacement. This
     * should get rid of the 0's we put in for registers in the callback.
     */
    *ep = expr_simplify(*ep, NULL);
    /* e = *ep; */

    return 2;
}

/* Calculate the displacement length, if possible.
 * Takes several extra inputs so it can be used by both 32-bit and 16-bit
 * expressions:
 *  wordsize=2 for 16-bit, =4 for 32-bit.
 *  noreg=1 if the *ModRM byte* has no registers used.
 *  isbpreg=1 if BP/EBP is the *only* register used within the *ModRM byte*.
 */
/*@-nullstate@*/
static int
x86_checkea_calc_displen(expr **ep, unsigned int wordsize, int noreg,
			 int isbpreg, unsigned char *displen,
			 unsigned char *modrm, unsigned char *v_modrm)
{
    expr *e = *ep;
    const intnum *intn;
    long dispval;

    *v_modrm = 0;	/* default to not yet valid */

    switch (*displen) {
	case 0:
	    /* the displacement length hasn't been forced, try to
	     * determine what it is.
	     */
	    if (noreg) {
		/* no register in ModRM expression, so it must be disp16/32,
		 * and as the Mod bits are set to 0 by the caller, we're done
		 * with the ModRM byte.
		 */
		*displen = wordsize;
		*v_modrm = 1;
		break;
	    } else if (isbpreg) {
		/* for BP/EBP, there *must* be a displacement value, but we
		 * may not know the size (8 or 16/32) for sure right now.
		 * We can't leave displen at 0, because that just means
		 * unknown displacement, including none.
		 */
		*displen = 0xff;
	    }

	    intn = expr_get_intnum(ep, NULL);
	    if (!intn) {
		/* expr still has unknown values: assume 16/32-bit disp */
		*displen = wordsize;
		*modrm |= 0200;
		*v_modrm = 1;
		break;
	    }	

	    /* make sure the displacement will fit in 16/32 bits if unsigned,
	     * and 8 bits if signed.
	     */
	    if (!intnum_check_size(intn, (size_t)wordsize, 0) &&
		!intnum_check_size(intn, 1, 1)) {
		Error(e->line, _("invalid effective address"));
		return 0;
	    }

	    /* don't try to find out what size displacement we have if
	     * displen is known.
	     */
	    if (*displen != 0 && *displen != 0xff) {
		if (*displen == 1)
		    *modrm |= 0100;
		else
		    *modrm |= 0200;
		*v_modrm = 1;
		break;
	    }

	    /* Don't worry about overflows here (it's already guaranteed
	     * to be 16/32 or 8 bits).
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
		 * Don't do this if we came from isbpreg check above, so
		 * check *displen.
		 */
		expr_delete(e);
		*ep = (expr *)NULL;
	    } else if (dispval >= -128 && dispval <= 127) {
		/* It fits into a signed byte */
		*displen = 1;
		*modrm |= 0100;
	    } else {
		/* It's a 16/32-bit displacement */
		*displen = wordsize;
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
	    /* TODO: Add optional warning here about byte not being valid
	     * override in noreg case.
	     */
	    if (!noreg)
		*modrm |= 0100;
	    *v_modrm = 1;
	    break;
	case 2:
	case 4:
	    if (wordsize != *displen) {
		Error(e->line,
		      _("invalid effective address (displacement size)"));
		return 0;
	    }
	    /* TODO: Add optional warning here about 2/4 not being valid
	     * override in noreg case.
	     */
	    if (!noreg)
		*modrm |= 0200;
	    *v_modrm = 1;
	    break;
	default:
	    /* we shouldn't ever get any other size! */
	    InternalError(_("strange EA displacement size"));
    }

    return 1;
}
/*@=nullstate@*/

static int
x86_expr_checkea_getregsize_callback(ExprItem *ei, void *d)
{
    unsigned char *addrsize = (unsigned char *)d;

    if (ei->type == EXPR_REG) {
	*addrsize = (unsigned char)ei->data.reg & ~7;
	return 1;
    } else
	return 0;
}

int
x86_expr_checkea(expr **ep, unsigned char *addrsize, unsigned char bits,
		 unsigned char nosplit, unsigned char *displen,
		 unsigned char *modrm, unsigned char *v_modrm,
		 unsigned char *n_modrm, unsigned char *sib,
		 unsigned char *v_sib, unsigned char *n_sib,
		 calc_bc_dist_func calc_bc_dist)
{
    expr *e = *ep;

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
		if (!expr_traverse_leaves_in(e, addrsize,
					     x86_expr_checkea_getregsize_callback))
		    *addrsize = bits;
		/* TODO: Add optional warning here if switched address size
		 * from bits setting just by register use.. eg [ax] in
		 * 32-bit mode would generate a warning.
		 */
	}
    }

    if (*addrsize == 32 && ((*n_modrm && !*v_modrm) || (*n_sib && !*v_sib))) {
	int i;
	typedef enum {
	    REG32_NONE = -1,
	    REG32_EAX = 0,
	    REG32_ECX = 1,
	    REG32_EDX = 2,
	    REG32_EBX = 3,
	    REG32_ESP = 4,
	    REG32_EBP = 5,
	    REG32_ESI = 6,
	    REG32_EDI = 7
	} reg32type;
	int reg32mult[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	int basereg = REG32_NONE;	/* "base" register (for SIB) */
	int indexreg = REG32_NONE;	/* "index" register (for SIB) */
	
	switch (x86_expr_checkea_getregusage(ep, &indexreg, reg32mult,
					     x86_expr_checkea_get_reg32,
					     calc_bc_dist)) {
	    case 0:
		e = *ep;
		Error(e->line, _("invalid effective address"));
		return 0;
	    case 1:
		return 1;
	    default:
		e = *ep;
		break;
	}

	/* If indexreg mult is 0, discard it.
	 * This is possible because of the way indexreg is found in
	 * expr_checkea_getregusage().
	 */
	if (indexreg != REG32_NONE && reg32mult[indexreg] == 0)
	    indexreg = REG32_NONE;

	/* Find a basereg (*1, but not indexreg), if there is one.
	 * Also, if an indexreg hasn't been assigned, try to find one.
	 * Meanwhile, check to make sure there's no negative register mults.
	 */
	for (i=0; i<8; i++) {
	    if (reg32mult[i] < 0) {
		Error(e->line, _("invalid effective address"));
		return 0;
	    }
	    if (i != indexreg && reg32mult[i] == 1 && basereg == REG32_NONE)
		basereg = i;
	    else if (indexreg == REG32_NONE && reg32mult[i] > 0)
		indexreg = i;
	}

	/* Handle certain special cases of indexreg mults when basereg is
	 * empty.
	 */
	if (indexreg != REG32_NONE && basereg == REG32_NONE)
	    switch (reg32mult[indexreg]) {
		case 1:
		    /* Only optimize this way if nosplit wasn't specified */
		    if (!nosplit) {
			basereg = indexreg;
			indexreg = -1;
		    }
		    break;
		case 2:
		    /* Only split if nosplit wasn't specified */
		    if (!nosplit) {
			basereg = indexreg;
			reg32mult[indexreg] = 1;
		    }
		    break;
		case 3:
		case 5:
		case 9:
		    basereg = indexreg;
		    reg32mult[indexreg]--;
		    break;
	    }

	/* Make sure there's no other registers than the basereg and indexreg
	 * we just found.
	 */
	for (i=0; i<8; i++)
	    if (i != basereg && i != indexreg && reg32mult[i] != 0) {
		Error(e->line, _("invalid effective address"));
		return 0;
	    }

	/* Check the index multiplier value for validity if present. */
	if (indexreg != REG32_NONE && reg32mult[indexreg] != 1 &&
	    reg32mult[indexreg] != 2 && reg32mult[indexreg] != 4 &&
	    reg32mult[indexreg] != 8) {
	    Error(e->line, _("invalid effective address"));
	    return 0;
	}

	/* ESP is not a legal indexreg. */
	if (indexreg == REG32_ESP) {
	    /* If mult>1 or basereg is ESP also, there's no way to make it
	     * legal.
	     */
	    if (reg32mult[REG32_ESP] > 1 || basereg == REG32_ESP) {
		Error(e->line, _("invalid effective address"));
		return 0;
	    }
	    /* If mult==1 and basereg is not ESP, swap indexreg w/basereg. */
	    indexreg = basereg;
	    basereg = REG32_ESP;
	}

	/* At this point, we know the base and index registers and that the
	 * memory expression is (essentially) valid.  Now build the ModRM and
	 * (optional) SIB bytes.
	 */

	/* First determine R/M (Mod is later determined from disp size) */
	*n_modrm = 1;	/* we always need ModRM */
	if (basereg == REG32_NONE && indexreg == REG32_NONE) {
	    /* just a disp32 */
	    *modrm |= 5;
	    *sib = 0;
	    *v_sib = 0;
	    *n_sib = 0;
	} else if (indexreg == REG32_NONE) {
	    /* basereg only */
	    *modrm |= basereg;
	    /* we don't need an SIB *unless* basereg is ESP */
	    if (basereg == REG32_ESP)
		*n_sib = 1;
	    else {
		*sib = 0;
		*v_sib = 0;
		*n_sib = 0;
	    }
	} else {
	    /* index or both base and index */
	    *modrm |= 4;
	    *n_sib = 1;
	}

	/* Determine SIB if needed */
	if (*n_sib == 1) {
	    *sib = 0;	/* start with 0 */

	    /* Special case: no basereg (only happens in disp32[index] case) */
	    if (basereg == REG32_NONE)
		*sib |= 5;
	    else
		*sib |= basereg & 7;	/* &7 to sanity check */
	    
	    /* Put in indexreg, checking for none case */
	    if (indexreg == REG32_NONE)
		*sib |= 040;
		/* Any scale field is valid, just leave at 0. */
	    else {
		*sib |= ((unsigned int)indexreg & 7) << 3;
		/* Set scale field, 1 case -> 0, so don't bother. */
		switch (reg32mult[indexreg]) {
		    case 2:
			*sib |= 0100;
			break;
		    case 4:
			*sib |= 0200;
			break;
		    case 8:
			*sib |= 0300;
			break;
		}
	    }

	    *v_sib = 1;	/* Done with SIB */
	}

	/* Calculate displacement length (if possible) */
	return x86_checkea_calc_displen(ep, 4, basereg == REG32_NONE,
					basereg == REG32_EBP &&
					    indexreg == REG32_NONE, displen,
					modrm, v_modrm);
    } else if (*addrsize == 16 && *n_modrm && !*v_modrm) {
	static const unsigned char modrm16[16] = {
	    0006 /* disp16  */, 0007 /* [BX]    */, 0004 /* [SI]    */,
	    0000 /* [BX+SI] */, 0005 /* [DI]    */, 0001 /* [BX+DI] */,
	    0377 /* invalid */, 0377 /* invalid */, 0006 /* [BP]+d  */,
	    0377 /* invalid */, 0002 /* [BP+SI] */, 0377 /* invalid */,
	    0003 /* [BP+DI] */, 0377 /* invalid */, 0377 /* invalid */,
	    0377 /* invalid */
	};
	x86_checkea_reg16_data reg16mult = {0, 0, 0, 0};
	enum {
	    HAVE_NONE = 0,
	    HAVE_BX = 1<<0,
	    HAVE_SI = 1<<1,
	    HAVE_DI = 1<<2,
	    HAVE_BP = 1<<3
	} havereg = HAVE_NONE;

	/* 16-bit cannot have SIB */
	*sib = 0;
	*v_sib = 0;
	*n_sib = 0;

	switch (x86_expr_checkea_getregusage(ep, (int *)NULL, &reg16mult,
					     x86_expr_checkea_get_reg16,
					     calc_bc_dist)) {
	    case 0:
		e = *ep;
		Error(e->line, _("invalid effective address"));
		return 0;
	    case 1:
		return 1;
	    default:
		e = *ep;
		break;
	}

	/* reg multipliers not 0 or 1 are illegal. */
	if (reg16mult.bx & ~1 || reg16mult.si & ~1 || reg16mult.di & ~1 ||
	    reg16mult.bp & ~1) {
	    Error(e->line, _("invalid effective address"));
	    return 0;
	}

	/* Set havereg appropriately */
	if (reg16mult.bx > 0)
	    havereg |= HAVE_BX;
	if (reg16mult.si > 0)
	    havereg |= HAVE_SI;
	if (reg16mult.di > 0)
	    havereg |= HAVE_DI;
	if (reg16mult.bp > 0)
	    havereg |= HAVE_BP;

	/* Check the modrm value for invalid combinations. */
	if (modrm16[havereg] & 0070) {
	    Error(e->line, _("invalid effective address"));
	    return 0;
	}

	/* Set ModRM byte for registers */
	*modrm |= modrm16[havereg];

	/* Calculate displacement length (if possible) */
	return x86_checkea_calc_displen(ep, 2, havereg == HAVE_NONE,
					havereg == HAVE_BP, displen, modrm,
					v_modrm);
    } else if (!*n_modrm && !*n_sib) {
	/* Special case for MOV MemOffs opcode: displacement but no modrm. */
	if (*addrsize == 32)
	    *displen = 4;
	else if (*addrsize == 16)
	    *displen = 2;
    }
    return 1;
}

int
x86_floatnum_tobytes(const floatnum *flt, unsigned char **bufp,
		     unsigned long valsize, const expr *e)
{
    int fltret;

    if (!floatnum_check_size(flt, (size_t)valsize)) {
	Error(e->line, _("invalid floating point constant size"));
	return 1;
    }

    fltret = floatnum_get_sized(flt, *bufp, (size_t)valsize);
    if (fltret < 0) {
	Error(e->line, _("underflow in floating point expression"));
	return 1;
    }
    if (fltret > 0) {
	Error(e->line, _("overflow in floating point expression"));
	return 1;
    }
    *bufp += valsize;
    return 0;
}
