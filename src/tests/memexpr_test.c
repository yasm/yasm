/* $IdPath$
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "util.h"

#include "check.h"

#include "bitvect.h"

#include "errwarn.h"

#include "expr.h"
#include "intnum.h"
#include "floatnum.h"

#include "bytecode.h"
#include "arch.h"
#include "x86arch.h"

typedef enum {
    REG_AX = 0,
    REG_CX = 1,
    REG_DX = 2,
    REG_BX = 3,
    REG_SP = 4,
    REG_BP = 5,
    REG_SI = 6,
    REG_DI = 7
} reg16type;

/* Memory expression building functions.
 * These try to exactly match how a parser will build up the expr for _in,
 * and exactly what the output expr should be for _out.
 */

/* [5] */
static expr *
gen_5_in(void)
{
    return expr_new_ident(ExprInt(intnum_new_uint(5)));
}
#define gen_5_out	gen_5_in
/* [1.2] */
static expr *
gen_1pt2_in(void)
{
    return expr_new_ident(ExprFloat(floatnum_new("1.2")));
}
/* No _out, it's invalid */
/* [ecx] */
static expr *
gen_ecx_in(void)
{
    return expr_new_ident(ExprReg(REG_CX, 32));
}
#define gen_ecx_out	NULL
/* [di] */
static expr *
gen_di_in(void)
{
    return expr_new_ident(ExprReg(REG_DI, 16));
}
#define gen_di_out	NULL
/* [di-si+si+126] */
static expr *
gen_dimsipsip126_in(void)
{
    return expr_new_tree(
	expr_new_tree(
	    expr_new_tree(
		expr_new_ident(ExprReg(REG_DI, 16)),
		EXPR_SUB,
		expr_new_ident(ExprReg(REG_SI, 16))),
	    EXPR_ADD,
	    expr_new_ident(ExprReg(REG_SI, 16))),
	EXPR_ADD,
	expr_new_ident(ExprInt(intnum_new_uint(126))));
}
#define gen_dimsipsip126_out	NULL
/* [bx-(bx-di)+bx-2] */
static expr *
gen_bxmqbxmdiqpbxm2_in(void)
{
    return expr_new_tree(
	expr_new_tree(
	    expr_new_tree(
		expr_new_ident(ExprReg(REG_BX, 16)),
		EXPR_SUB,
		expr_new_tree(
		    expr_new_ident(ExprReg(REG_BX, 16)),
		    EXPR_SUB,
		    expr_new_ident(ExprReg(REG_DI, 16)))),
	    EXPR_ADD,
	    expr_new_ident(ExprReg(REG_BX, 16))),
	EXPR_SUB,
	expr_new_ident(ExprInt(intnum_new_uint(2))));
}
static expr *
gen_bxmqbxmdiqpbxm2_out(void)
{
    return expr_new_ident(ExprInt(intnum_new_int(-2)));
}
/* [bp] */
static expr *
gen_bp_in(void)
{
    return expr_new_ident(ExprReg(REG_BP, 16));
}
#define gen_bp_out		NULL
/* [bp*1+500] */
static expr *
gen_bpx1p500_in(void)
{
    return expr_new_tree(
	expr_new_tree(
	    expr_new_ident(ExprReg(REG_BP, 16)),
	    EXPR_MUL,
	    expr_new_ident(ExprInt(intnum_new_uint(1)))),
	EXPR_ADD,
	expr_new_ident(ExprInt(intnum_new_uint(500))));
}
static expr *
gen_bpx1p500_out(void)
{
    return expr_new_ident(ExprInt(intnum_new_uint(500)));
}

typedef struct CheckEA_InOut {
    /* Function to generate input/output expr. */
    expr *(*expr_gen)(void);
    unsigned char addrsize;
    unsigned char bits;
    unsigned char nosplit;
    unsigned char displen;
    unsigned char modrm;
    unsigned char v_modrm;
    unsigned char n_modrm;
    unsigned char sib;
    unsigned char v_sib;
    unsigned char n_sib;
} CheckEA_InOut;

typedef struct CheckEA_Entry {
    const char *ascii;	    /* Text description of input */
    CheckEA_InOut in;	    /* Input Parameter Values */
    int retval;		    /* Return value */
    CheckEA_InOut out;	    /* Correct output Parameter Values
			       (N/A if retval=0) */
} CheckEA_Entry;

/* Values used for tests */
static CheckEA_Entry bits16_vals[] = {
    {
	"[5]",
	{gen_5_in ,  0, 16, 0, 0,    0, 0, 1,    0, 0, 0xff},
	1,
	{gen_5_out, 16, 16, 0, 2, 0x06, 1, 1,    0, 0,    0}
    },
    {
	"a16 [5]",
	{gen_5_in , 16, 16, 0, 0,    0, 0, 1,    0, 0, 0xff},
	1,
	{gen_5_out, 16, 16, 0, 2, 0x06, 1, 1,    0, 0,    0}
    },
    {
	"a32 [5]",
	{gen_5_in , 32, 16, 0, 0,    0, 0, 1,    0, 0, 0xff},
	1,
	{gen_5_out, 32, 16, 0, 4, 0x05, 1, 1, 0x25, 1,    1}
    },
    {
	"[word 5]",
	{gen_5_in ,  0, 16, 0, 2,    0, 0, 1,    0, 0, 0xff},
	1,
	{gen_5_out, 16, 16, 0, 2, 0x06, 1, 1,    0, 0,    0}
    },
    {
	"[dword 5]",
	{gen_5_in ,  0, 16, 0, 4,    0, 0, 1,    0, 0, 0xff},
	1,
	{gen_5_out, 32, 16, 0, 4, 0x05, 1, 1, 0x25, 1,    1}
    },
    {
	"a16 [dword 5]",
	{gen_5_in, 16, 16, 0, 4,    0, 0, 1,    0, 0, 0xff},
	0,
	{NULL    ,  0,  0, 0, 0,    0, 0, 0,    0, 0,    0}
    },
    /* should error */
    {
	"[di+1.2]",
	{gen_1pt2_in,  0, 16, 0, 0,    0, 0, 1,    0, 0, 0xff},
	0,
	{NULL       ,  0,  0, 0, 0,    0, 0, 0,    0, 0,    0}
    },
    {
	"[ecx]",
	{gen_ecx_in ,  0, 16, 0, 0,    0, 0, 1,    0, 0, 0xff},
	1,
	{gen_ecx_out, 32, 16, 0, 0, 0x01, 1, 1,    0, 0,    0}
    },
    /* should error */
    {
	"a16 [ecx]",
	{gen_ecx_in, 16, 16, 0, 0,    0, 0, 1,    0, 0, 0xff},
	0,
	{NULL      ,  0,  0, 0, 0,    0, 0, 0,    0, 0,    0}
    },
    {
	"[di]",
	{gen_di_in ,  0, 16, 0, 0,    0, 0, 1,    0, 0, 0xff},
	1,
	{gen_di_out, 16, 16, 0, 0, 0x05, 1, 1,    0, 0,    0}
    },
    {
	"[di-si+si+126]",
	{gen_dimsipsip126_in ,  0, 16, 0, 0,    0, 0, 1,    0, 0, 0xff},
	1,
	{gen_dimsipsip126_out, 16, 16, 0, 1, 0x45, 1, 1,    0, 0,    0}
    },
    {
	"[bx-(bx-di)+bx-2]",
	{gen_bxmqbxmdiqpbxm2_in ,  0, 16, 0, 0,    0, 0, 1,    0, 0, 0xff},
	1,
	{gen_bxmqbxmdiqpbxm2_out, 16, 16, 0, 1, 0x41, 1, 1,    0, 0,    0}
    },
    {
	"[bp]",
	{gen_bp_in ,  0, 16, 0, 0,    0, 0, 1,    0, 0, 0xff},
	1,
	{gen_bp_out, 16, 16, 0, 1, 0x46, 1, 1,    0, 0,    0}
    },
    {
	"[bp*1+500]",
	{gen_bpx1p500_in ,  0, 16, 0, 0,    0, 0, 1,    0, 0, 0xff},
	1,
	{gen_bpx1p500_out, 16, 16, 0, 2, 0x86, 1, 1,    0, 0,    0}
    },
};

/* input expression */
expr *expn;

/* failure messages */
static char result_msg[1024];

int error_triggered;

/* Replace errwarn functions */
void InternalError_(const char *file, unsigned int line, const char *msg)
{
    exit(EXIT_FAILURE);
}

void
Fatal(fatal_num num)
{
    exit(EXIT_FAILURE);
}

void
Error(const char *msg, ...)
{
    error_triggered = 1;
}

void
Warning(const char *msg, ...)
{
}

void
ErrorAt(const char *filename, unsigned long line, const char *fmt, ...)
{
    error_triggered = 1;
}

void
WarningAt(const char *filename, unsigned long line, const char *fmt, ...)
{
}

static int
x86_checkea_check(CheckEA_Entry *val)
{
    CheckEA_InOut chk = val->in;    /* local structure copy of inputs */
    int retval;

    error_triggered = 0;

    /* execute function and check return value */
    retval = x86_expr_checkea(&expn, &chk.addrsize, chk.bits, chk.nosplit,
			      &chk.displen, &chk.modrm, &chk.v_modrm,
			      &chk.n_modrm, &chk.sib, &chk.v_sib, &chk.n_sib);
    if (retval != val->retval) {
	sprintf(result_msg, "%s: incorrect %s (expected %d, got %d)",
		val->ascii, "return value", val->retval, retval);
	return 1;
    }

    /* If returned 0 (failure), check to see if ErrorAt() was called */
    if (retval == 0) {
	if (error_triggered == 0) {
	    sprintf(result_msg, "%s: didn't call ErrorAt() and returned 0",
		    val->ascii);
	    return 1;
	}

	return 0;	/* don't check other return values */
    }

    /* check expr result */
    /* TODO */

    /* Check other outputs */
    if (chk.addrsize != val->out.addrsize) {
	sprintf(result_msg, "%s: incorrect %s (expected %d, got %d)",
		val->ascii, "addrsize", (int)val->out.addrsize,
		(int)chk.addrsize);
	return 1;
    }
    if (chk.displen != val->out.displen) {
	sprintf(result_msg, "%s: incorrect %s (expected %d, got %d)",
		val->ascii, "displen", (int)val->out.displen,
		(int)chk.displen);
	return 1;
    }
    if (chk.modrm != val->out.modrm) {
	sprintf(result_msg, "%s: incorrect %s (expected %03o, got %03o)",
		val->ascii, "modrm", (int)val->out.modrm, (int)chk.modrm);
	return 1;
    }
    if (chk.v_modrm != val->out.v_modrm) {
	sprintf(result_msg, "%s: incorrect %s (expected %d, got %d)",
		val->ascii, "v_modrm", (int)val->out.v_modrm,
		(int)chk.v_modrm);
	return 1;
    }
    if (chk.n_modrm != val->out.n_modrm) {
	sprintf(result_msg, "%s: incorrect %s (expected %d, got %d)",
		val->ascii, "n_modrm", (int)val->out.n_modrm,
		(int)chk.n_modrm);
	return 1;
    }
    if (chk.sib != val->out.sib) {
	sprintf(result_msg, "%s: incorrect %s (expected %03o, got %03o)",
		val->ascii, "sib", (int)val->out.sib, (int)chk.sib);
	return 1;
    }
    if (chk.v_sib != val->out.v_sib) {
	sprintf(result_msg, "%s: incorrect %s (expected %d, got %d)",
		val->ascii, "v_sib", (int)val->out.v_sib, (int)chk.v_sib);
	return 1;
    }
    if (chk.n_sib != val->out.n_sib) {
	sprintf(result_msg, "%s: incorrect %s (expected %x, got %x)",
		val->ascii, "n_sib", (int)val->out.n_sib, (int)chk.n_sib);
	return 1;
    }
    return 0;
}

START_TEST(test_x86_checkea_bits16)
{
    CheckEA_Entry *vals = bits16_vals;
    int i, num = sizeof(bits16_vals)/sizeof(CheckEA_Entry);

    for (i=0; i<num; i++) {
	expn = vals[i].in.expr_gen();
	fail_unless(x86_checkea_check(&vals[i]) == 0, result_msg);
	expr_delete(expn);
    }
}
END_TEST

static Suite *
memexpr_suite(void)
{
    Suite *s = suite_create("memexpr");
    TCase *tc_x86_checkea = tcase_create("x86_checkea");

    suite_add_tcase(s, tc_x86_checkea);
    tcase_add_test(tc_x86_checkea, test_x86_checkea_bits16);

    return s;
}

int
main(void)
{
    int nf;
    Suite *s = memexpr_suite();
    SRunner *sr = srunner_create(s);
    BitVector_Boot();
    srunner_run_all(sr, CRNORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);
    suite_free(s);
    return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
