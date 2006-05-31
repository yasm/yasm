/*
 * CodeView debugging formats implementation for Yasm
 *
 *  Copyright (C) 2006  Peter Johnson
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

#include "cv-dbgfmt.h"

yasm_dbgfmt_module yasm_cv8_LTX_dbgfmt;


static /*@null@*/ /*@only@*/ yasm_dbgfmt *
cv_dbgfmt_create(yasm_object *object, yasm_objfmt *of, yasm_arch *a,
		 yasm_dbgfmt_module *module, int version)
{
    yasm_dbgfmt_cv *dbgfmt_cv = yasm_xmalloc(sizeof(yasm_dbgfmt_cv));
    size_t i;

    dbgfmt_cv->dbgfmt.module = module;

    dbgfmt_cv->object = object;
    dbgfmt_cv->symtab = yasm_object_get_symtab(object);
    dbgfmt_cv->linemap = yasm_object_get_linemap(object);
    dbgfmt_cv->arch = a;

    dbgfmt_cv->filenames_allocated = 32;
    dbgfmt_cv->filenames_size = 0;
    dbgfmt_cv->filenames =
	yasm_xmalloc(sizeof(cv_filename)*dbgfmt_cv->filenames_allocated);
    for (i=0; i<dbgfmt_cv->filenames_allocated; i++) {
	dbgfmt_cv->filenames[i].pathname = NULL;
	dbgfmt_cv->filenames[i].filename = NULL;
	dbgfmt_cv->filenames[i].str_off = 0;
	dbgfmt_cv->filenames[i].info_off = 0;
    }

    dbgfmt_cv->version = version;

    return (yasm_dbgfmt *)dbgfmt_cv;
}

static /*@null@*/ /*@only@*/ yasm_dbgfmt *
cv8_dbgfmt_create(yasm_object *object, yasm_objfmt *of, yasm_arch *a)
{
    return cv_dbgfmt_create(object, of, a, &yasm_cv8_LTX_dbgfmt, 8);
}

static void
cv_dbgfmt_destroy(/*@only@*/ yasm_dbgfmt *dbgfmt)
{
    yasm_dbgfmt_cv *dbgfmt_cv = (yasm_dbgfmt_cv *)dbgfmt;
    size_t i;
    for (i=0; i<dbgfmt_cv->filenames_size; i++) {
	if (dbgfmt_cv->filenames[i].pathname)
	    yasm_xfree(dbgfmt_cv->filenames[i].pathname);
    }
    yasm_xfree(dbgfmt_cv->filenames);
    yasm_xfree(dbgfmt);
}

/* Add a bytecode to a section, updating offset on insertion;
 * no optimization necessary.
 */
yasm_bytecode *
yasm_cv__append_bc(yasm_section *sect, yasm_bytecode *bc)
{
    yasm_bytecode *precbc = yasm_section_bcs_last(sect);
    bc->offset = precbc ? precbc->offset + precbc->len : 0;
    yasm_section_bcs_append(sect, bc);
    return precbc;
}

static void
cv_dbgfmt_generate(yasm_dbgfmt *dbgfmt, yasm_errwarns *errwarns)
{
    yasm_dbgfmt_cv *dbgfmt_cv = (yasm_dbgfmt_cv *)dbgfmt;

    yasm_cv__generate_symline(dbgfmt_cv, errwarns);

    yasm_cv__generate_type(dbgfmt_cv);
}

static int
cv_dbgfmt_directive(yasm_dbgfmt *dbgfmt, const char *name,
		    yasm_section *sect, yasm_valparamhead *valparams,
		    unsigned long line)
{
    return 1;	    /* TODO */
}

/* Define dbgfmt structure -- see dbgfmt.h for details */
yasm_dbgfmt_module yasm_cv8_LTX_dbgfmt = {
    "CodeView debugging format for VC8",
    "cv8",
    cv8_dbgfmt_create,
    cv_dbgfmt_destroy,
    cv_dbgfmt_directive,
    cv_dbgfmt_generate
};
