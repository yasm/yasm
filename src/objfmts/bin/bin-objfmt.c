/*
 * Flat-format binary object format
 *
 *  Copyright (C) 2002  Peter Johnson
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

#include "globals.h"
#include "errwarn.h"
#include "intnum.h"
#include "expr.h"
#include "symrec.h"

#include "section.h"
#include "objfmt.h"


static void
bin_objfmt_initialize(const char *in_filename, const char *obj_filename)
{
}

static void
bin_objfmt_output(FILE *f, sectionhead *sections)
{
}

static void
bin_objfmt_cleanup(void)
{
}

static /*@dependent@*/ /*@null@*/ section *
bin_objfmt_sections_switch(sectionhead *headp, valparamhead *valparams,
			   /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    valparam *vp;
    section *retval;
    int isnew;
    unsigned long start;
    char *sectname;
    /*@null@*/ void *data = NULL;
    int resonly = 0;

    if ((vp = vps_first(valparams)) && !vp->param && vp->val != NULL) {
	/* If it's the first section output (.text) start at 0, otherwise
	 * make sure the start is > 128.
	 */
	sectname = vp->val;
	if (strcmp(sectname, ".text") == 0)
	    start = 0;
	else if (strcmp(sectname, ".data") == 0)
	    start = 200;
	else if (strcmp(sectname, ".bss") == 0) {
	    start = 200;
	    resonly = 1;
	} else {
	    /* other section names not recognized. */
	    Error(_("segment name `%s' not recognized"), sectname);
	    return NULL;
	}

	/* Check for ALIGN qualifier */
	while ((vp = vps_next(vp))) {
	    if (strcasecmp(vp->val, "align") == 0 && vp->param) {
		/*@dependent@*/ /*@null@*/ const intnum *align;
		unsigned long alignval;
		unsigned long bitcnt;

		if (strcmp(sectname, ".text") == 0) {
		    Error(_("cannot specify an alignment to the `%s' section"),
			  sectname);
		    return NULL;
		}
		
		align = expr_get_intnum(&vp->param);
		if (!align) {
		    Error(_("argument to `%s' is not a power of two"),
			  vp->val);
		    return NULL;
		}
		alignval = intnum_get_uint(align);

		/* Check to see if alignval is a power of two.
		 * This can be checked by seeing if only one bit is set.
		 */
		BitCount(bitcnt, alignval);
		if (bitcnt > 1) {
		    Error(_("argument to `%s' is not a power of two"),
			  vp->val);
		    return NULL;
		}

		/* Point data to (a copy of) alignval. */
		data = xmalloc(sizeof(unsigned long));
		*((unsigned long *)data) = alignval;
	    }
	}

	retval = sections_switch_general(headp, sectname, start, data, resonly,
					 &isnew);
	if (isnew)
	    symrec_define_label(sectname, retval, (bytecode *)NULL, 1);
	return retval;
    } else
	return NULL;
}

static void
bin_objfmt_section_data_delete(/*@only@*/ void *data)
{
    xfree(data);
}

static void
bin_objfmt_section_data_print(FILE *f, /*@null@*/ void *data)
{
    if (data)
	fprintf(f, "%*s%p\n", indent_level, "", data);
    else
	fprintf(f, "%*s(none)\n", indent_level, "");
}

static /*@null@*/ void *
bin_objfmt_extern_data_new(const char *name, /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    return NULL;
}

static /*@null@*/ void *
bin_objfmt_global_data_new(const char *name, /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    return NULL;
}

static /*@null@*/ void *
bin_objfmt_common_data_new(const char *name,
			   /*@unused@*/ /*@only@*/ expr *size,
			   /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    Error(_("binary object format does not support common variables"));
    return NULL;
}

static void *
bin_objfmt_declare_data_copy(SymVisibility vis, /*@only@*/ void *data)
{
    if (vis == SYM_COMMON) {
	return expr_copy(data);
    } else {
	InternalError(_("Unrecognized data in bin objfmt declare_data_copy"));
	return NULL;
    }
}

static void
bin_objfmt_declare_data_delete(SymVisibility vis, /*@only@*/ void *data)
{
    if (vis == SYM_COMMON) {
	expr_delete(data);
    } else {
	InternalError(_("Unrecognized data in bin objfmt declare_data_delete"));
	xfree(data);
    }
}

static void
bin_objfmt_declare_data_print(FILE *f, SymVisibility vis,
			      /*@null@*/ void *data)
{
    if (vis == SYM_COMMON) {
	fprintf(f, "%*sSize=", indent_level, "");
	expr_print(f, data);
	fprintf(f, "\n");
    } else {
	fprintf(f, "%*s(none)\n", indent_level, "");
    }
}

static int
bin_objfmt_directive(const char *name, valparamhead *valparams,
		     /*@null@*/ valparamhead *objext_valparams,
		     sectionhead *headp)
{
    section *sect;
    valparam *vp;

    if (strcasecmp(name, "org") == 0) {
	/*@dependent@*/ /*@null@*/ const intnum *start = NULL;

	/* ORG takes just a simple integer as param */
	vp = vps_first(valparams);
	if (vp->val) {
	    Error(_("argument to ORG should be numeric"));
	    return 0;
	} else if (vp->param)
	    start = expr_get_intnum(&vp->param);

	if (!start) {
	    Error(_("argument to ORG should be numeric"));
	    return 0;
	}

	/* ORG changes the start of the .text section */
	sect = sections_find_general(headp, ".text");
	if (!sect)
	    InternalError(_("bin objfmt: .text section does not exist before ORG is called?"));
	section_set_start(sect, intnum_get_uint(start));

	return 0;	    /* directive recognized */
    } else
	return 1;	    /* directive unrecognized */
}

/* Define objfmt structure -- see objfmt.h for details */
objfmt bin_objfmt = {
    "Flat format binary",
    "bin",
    NULL,
    ".text",
    16,
    bin_objfmt_initialize,
    bin_objfmt_output,
    bin_objfmt_cleanup,
    bin_objfmt_sections_switch,
    bin_objfmt_section_data_delete,
    bin_objfmt_section_data_print,
    bin_objfmt_extern_data_new,
    bin_objfmt_global_data_new,
    bin_objfmt_common_data_new,
    bin_objfmt_declare_data_copy,
    bin_objfmt_declare_data_delete,
    bin_objfmt_declare_data_print,
    bin_objfmt_directive
};
