/*
 * Debugging object format (used to debug object format module interface)
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

#include "globals.h"
#include "expr.h"
#include "symrec.h"

#include "section.h"
#include "objfmt.h"


/*@dependent@*/ FILE *dbg_f;

static void
dbg_objfmt_initialize(/*@dependent@*/ FILE *f)
{
    dbg_f = f;
    fprintf(dbg_f, "%*sinitialize(f)\n", indent_level, "");
}

static void
dbg_objfmt_finalize(void)
{
    fprintf(dbg_f, "%*sfinalize()\n", indent_level, "");
}

static /*@dependent@*/ /*@null@*/ section *
dbg_objfmt_sections_switch(sectionhead *headp, valparamhead *valparams,
			   /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    valparam *vp;
    section *retval;
    int isnew;

    fprintf(dbg_f, "%*ssections_switch(headp, ", indent_level, "");
    vps_print(dbg_f, valparams);
    fprintf(dbg_f, ", ");
    vps_print(dbg_f, objext_valparams);
    fprintf(dbg_f, "), returning ");

    if ((vp = vps_first(valparams)) && !vp->param && vp->val != NULL) {
	retval = sections_switch_general(headp, vp->val, NULL, 0, &isnew);
	if (isnew) {
	    fprintf(dbg_f, "(new) ");
	    symrec_define_label(vp->val, retval, (bytecode *)NULL, 1);
	}
	fprintf(dbg_f, "\"%s\" section\n", vp->val);
	return retval;
    } else {
	fprintf(dbg_f, "NULL\n");
	return NULL;
    }
}

static void
dbg_objfmt_section_data_delete(/*@only@*/ void *data)
{
    fprintf(dbg_f, "%*ssection_data_delete(%p)\n", indent_level, "", data);
    xfree(data);
}

static void
dbg_objfmt_section_data_print(FILE *f, /*@null@*/ void *data)
{
    if (data)
	fprintf(f, "%*s%p\n", indent_level, "", data);
    else
	fprintf(f, "%*s(none)\n", indent_level, "");
}

static /*@null@*/ void *
dbg_objfmt_extern_data_new(const char *name, /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    fprintf(dbg_f, "%*sextern_data_new(\"%s\", ", indent_level, "", name);
    vps_print(dbg_f, objext_valparams);
    fprintf(dbg_f, "), returning NULL\n");
    return NULL;
}

static /*@null@*/ void *
dbg_objfmt_global_data_new(const char *name, /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    fprintf(dbg_f, "%*sglobal_data_new(\"%s\", ", indent_level, "", name);
    vps_print(dbg_f, objext_valparams);
    fprintf(dbg_f, "), returning NULL\n");
    return NULL;
}

static /*@null@*/ void *
dbg_objfmt_common_data_new(const char *name, /*@only@*/ expr *size,
			   /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    fprintf(dbg_f, "%*scommon_data_new(\"%s\", ", indent_level, "", name);
    expr_print(dbg_f, size);
    fprintf(dbg_f, ", ");
    vps_print(dbg_f, objext_valparams);
    fprintf(dbg_f, "), returning ");
    expr_print(dbg_f, size);
    fprintf(dbg_f, "\n");
    return size;
}

static void
dbg_objfmt_declare_data_delete(SymVisibility vis, /*@only@*/ void *data)
{
    fprintf(dbg_f, "%*sdeclare_data_delete(", indent_level, "");
    switch (vis) {
	case SYM_LOCAL:
	    fprintf(dbg_f, "Local, ");
	    break;
	case SYM_GLOBAL:
	    fprintf(dbg_f, "Global, ");
	    break;
	case SYM_COMMON:
	    fprintf(dbg_f, "Common, ");
	    break;
	case SYM_EXTERN:
	    fprintf(dbg_f, "Extern, ");
	    break;
    }
    if (vis == SYM_COMMON) {
	expr_print(dbg_f, data);
	expr_delete(data);
    } else {
	fprintf(dbg_f, "%p", data);
	xfree(data);
    }
    fprintf(dbg_f, ")\n");
}

static void
dbg_objfmt_declare_data_print(FILE *f, SymVisibility vis,
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
dbg_objfmt_directive(const char *name, valparamhead *valparams,
		     /*@null@*/ valparamhead *objext_valparams)
{
    fprintf(dbg_f, "%*sdirective(\"%s\", ", indent_level, "", name);
    vps_print(dbg_f, valparams);
    fprintf(dbg_f, ", ");
    vps_print(dbg_f, objext_valparams);
    fprintf(dbg_f, "), returning 0 (recognized)\n");
    return 0;	    /* dbg format "recognizes" all directives */
}

/* Define objfmt structure -- see objfmt.h for details */
objfmt dbg_objfmt = {
    "Trace of all info passed to object format module",
    "dbg",
    "dbg",
    ".text",
    32,
    dbg_objfmt_initialize,
    dbg_objfmt_finalize,
    dbg_objfmt_sections_switch,
    dbg_objfmt_section_data_delete,
    dbg_objfmt_section_data_print,
    dbg_objfmt_extern_data_new,
    dbg_objfmt_global_data_new,
    dbg_objfmt_common_data_new,
    dbg_objfmt_declare_data_delete,
    dbg_objfmt_declare_data_print,
    dbg_objfmt_directive
};
