/*
 * NASM-style list format
 *
 *  Copyright (C) 2004  Peter Johnson
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
#include <util.h>
/*@unused@*/ RCSID("$Id$");

#define YASM_LIB_INTERNAL
#define YASM_BC_INTERNAL
#include <libyasm.h>


#define REGULAR_BUF_SIZE    1024

yasm_listfmt_module yasm_nasm_LTX_listfmt;

typedef struct nasm_listfmt_output_info {
    yasm_arch *arch;
} nasm_listfmt_output_info;

static /*@null@*/ /*@only@*/ yasm_listfmt *
nasm_listfmt_create(const char *in_filename, const char *obj_filename)
{
    yasm_listfmt_base *listfmt = yasm_xmalloc(sizeof(yasm_listfmt_base));
    listfmt->module = &yasm_nasm_LTX_listfmt;
    return (yasm_listfmt *)listfmt;
}

static void
nasm_listfmt_destroy(/*@only@*/ yasm_listfmt *listfmt)
{
    yasm_xfree(listfmt);
}

static int
nasm_listfmt_output_expr(yasm_expr **ep, unsigned char *buf, size_t destsize,
			 size_t valsize, int shift, unsigned long offset,
			 yasm_bytecode *bc, int rel, int warn,
			 /*@null@*/ void *d)
{
    /*@null@*/ nasm_listfmt_output_info *info = (nasm_listfmt_output_info *)d;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *intn;
    /*@dependent@*/ /*@null@*/ const yasm_floatnum *flt;

    assert(info != NULL);

    flt = yasm_expr_get_floatnum(ep);
    if (flt) {
	if (shift < 0)
	    yasm_internal_error(N_("attempting to negative shift a float"));
	return yasm_arch_floatnum_tobytes(info->arch, flt, buf, destsize,
					  valsize, (unsigned int)shift, warn,
					  bc->line);
    }

    intn = yasm_expr_get_intnum(ep, NULL);
    if (intn)
	return yasm_arch_intnum_tobytes(info->arch, intn, buf, destsize,
					valsize, shift, bc, rel, warn,
					bc->line);

    return 0;
}

static void
nasm_listfmt_output(yasm_listfmt *listfmt, FILE *f, yasm_linemap *linemap,
		    yasm_arch *arch)
{
    yasm_bytecode *bc;
    const char *source;
    unsigned long line = 1;
    unsigned long listline = 1;
    /*@only@*/ unsigned char *buf;
    nasm_listfmt_output_info info;

    info.arch = arch;

    buf = yasm_xmalloc(REGULAR_BUF_SIZE);

    while (!yasm_linemap_get_source(linemap, line, &bc, &source)) {
	if (!bc) {
	    fprintf(f, "%6lu %*s     %s\n", listline++, 28, "", source);
	} else while (bc && bc->line == line) {
	    /*@null@*/ /*@only@*/ unsigned char *bigbuf;
	    unsigned long size = REGULAR_BUF_SIZE;
	    unsigned long multiple;
	    unsigned long offset = bc->offset;
	    unsigned char *p;
	    int gap;

	    bigbuf = yasm_bc_tobytes(bc, buf, &size, &multiple, &gap, &info,
				     nasm_listfmt_output_expr, NULL);

	    p = bigbuf ? bigbuf : buf;
	    while (size > 0) {
		int i;

		fprintf(f, "%6lu %08lX ", listline++, offset);
		for (i=0; i<18 && size > 0; i+=2, size--)
		    fprintf(f, "%02X", *(p++));
		if (size > 0)
		    fprintf(f, "-");
		else {
		    if (multiple > 1) {
			fprintf(f, "<rept>");
			i += 6;
		    }
		    for (; i<18; i+=2)
			fprintf(f, "  ");
		    fprintf(f, " ");
		}
		if (source) {
		    fprintf(f, "    %s", source);
		    source = NULL;
		}
		fprintf(f, "\n");
	    }
	    
	    if (bigbuf)
		yasm_xfree(bigbuf);
	    bc = STAILQ_NEXT(bc, link);
	}
	line++;
    }

    yasm_xfree(buf);
}

/* Define listfmt structure -- see listfmt.h for details */
yasm_listfmt_module yasm_nasm_LTX_listfmt = {
    YASM_LISTFMT_VERSION,
    "NASM-style list format",
    "nasm",
    nasm_listfmt_create,
    nasm_listfmt_destroy,
    nasm_listfmt_output
};
