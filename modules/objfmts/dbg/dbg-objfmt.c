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
#include "errwarn.h"
#include "expr.h"
#include "symrec.h"

#include "section.h"
#include "objfmt.h"


/* Note that the functions here write to debug_file.  This is NOT legal for
 * other object formats to do--only the output() function can write to a file,
 * and only the file it's passed in its f parameter!
 */

static void
dbg_objfmt_initialize(const char *in_filename, const char *obj_filename)
{
    fprintf(debug_file, "%*sinitialize(\"%s\", \"%s\")\n", indent_level, "",
	    in_filename, obj_filename);
}

static void
dbg_objfmt_output(FILE *f, sectionhead *sections)
{
    fprintf(f, "%*soutput(f, sections->\n", indent_level, "");
    indent_level++;
    sections_print(f, sections);
    indent_level--;
    fprintf(f, "%*s)\n", indent_level, "");
    indent_level++;
    fprintf(f, "%*sSymbol Table:\n", indent_level, "");
    symrec_print_all(f);
    indent_level--;
}

static void
dbg_objfmt_cleanup(void)
{
    fprintf(debug_file, "%*scleanup()\n", indent_level, "");
}

static /*@dependent@*/ /*@null@*/ section *
dbg_objfmt_sections_switch(sectionhead *headp, valparamhead *valparams,
			   /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    valparam *vp;
    section *retval;
    int isnew;

    fprintf(debug_file, "%*ssections_switch(headp, ", indent_level, "");
    vps_print(debug_file, valparams);
    fprintf(debug_file, ", ");
    vps_print(debug_file, objext_valparams);
    fprintf(debug_file, "), returning ");

    if ((vp = vps_first(valparams)) && !vp->param && vp->val != NULL) {
	retval = sections_switch_general(headp, vp->val, NULL, 0, &isnew);
	if (isnew) {
	    fprintf(debug_file, "(new) ");
	    symrec_define_label(vp->val, retval, (bytecode *)NULL, 1);
	}
	fprintf(debug_file, "\"%s\" section\n", vp->val);
	return retval;
    } else {
	fprintf(debug_file, "NULL\n");
	return NULL;
    }
}

static void
dbg_objfmt_section_data_delete(/*@only@*/ void *data)
{
    fprintf(debug_file, "%*ssection_data_delete(%p)\n", indent_level, "", data);
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
    fprintf(debug_file, "%*sextern_data_new(\"%s\", ", indent_level, "", name);
    vps_print(debug_file, objext_valparams);
    fprintf(debug_file, "), returning NULL\n");
    return NULL;
}

static /*@null@*/ void *
dbg_objfmt_global_data_new(const char *name, /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    fprintf(debug_file, "%*sglobal_data_new(\"%s\", ", indent_level, "", name);
    vps_print(debug_file, objext_valparams);
    fprintf(debug_file, "), returning NULL\n");
    return NULL;
}

static /*@null@*/ void *
dbg_objfmt_common_data_new(const char *name, /*@only@*/ expr *size,
			   /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    fprintf(debug_file, "%*scommon_data_new(\"%s\", ", indent_level, "", name);
    expr_print(debug_file, size);
    fprintf(debug_file, ", ");
    vps_print(debug_file, objext_valparams);
    fprintf(debug_file, "), returning ");
    expr_print(debug_file, size);
    fprintf(debug_file, "\n");
    return size;
}

static void *
dbg_objfmt_declare_data_copy(SymVisibility vis, /*@only@*/ void *data)
{
    void *retval;

    fprintf(debug_file, "%*sdeclare_data_copy(", indent_level, "");
    switch (vis) {
	case SYM_LOCAL:
	    fprintf(debug_file, "Local, ");
	    break;
	case SYM_GLOBAL:
	    fprintf(debug_file, "Global, ");
	    break;
	case SYM_COMMON:
	    fprintf(debug_file, "Common, ");
	    break;
	case SYM_EXTERN:
	    fprintf(debug_file, "Extern, ");
	    break;
    }
    if (vis == SYM_COMMON) {
	expr_print(debug_file, data);
	retval = expr_copy(data);
    } else {
	fprintf(debug_file, "%p", data);
	InternalError(_("Trying to copy unrecognized objfmt data"));
    }
    fprintf(debug_file, "), returning copy\n");

    return retval;
}

static void
dbg_objfmt_declare_data_delete(SymVisibility vis, /*@only@*/ void *data)
{
    fprintf(debug_file, "%*sdeclare_data_delete(", indent_level, "");
    switch (vis) {
	case SYM_LOCAL:
	    fprintf(debug_file, "Local, ");
	    break;
	case SYM_GLOBAL:
	    fprintf(debug_file, "Global, ");
	    break;
	case SYM_COMMON:
	    fprintf(debug_file, "Common, ");
	    break;
	case SYM_EXTERN:
	    fprintf(debug_file, "Extern, ");
	    break;
    }
    if (vis == SYM_COMMON) {
	expr_print(debug_file, data);
	expr_delete(data);
    } else {
	fprintf(debug_file, "%p", data);
	xfree(data);
    }
    fprintf(debug_file, ")\n");
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
    fprintf(debug_file, "%*sdirective(\"%s\", ", indent_level, "", name);
    vps_print(debug_file, valparams);
    fprintf(debug_file, ", ");
    vps_print(debug_file, objext_valparams);
    fprintf(debug_file, "), returning 0 (recognized)\n");
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
    dbg_objfmt_output,
    dbg_objfmt_cleanup,
    dbg_objfmt_sections_switch,
    dbg_objfmt_section_data_delete,
    dbg_objfmt_section_data_print,
    dbg_objfmt_extern_data_new,
    dbg_objfmt_global_data_new,
    dbg_objfmt_common_data_new,
    dbg_objfmt_declare_data_copy,
    dbg_objfmt_declare_data_delete,
    dbg_objfmt_declare_data_print,
    dbg_objfmt_directive
};
