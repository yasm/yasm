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

#include "expr.h"
#include "symrec.h"

#include "section.h"
#include "objfmt.h"


static /*@dependent@*/ /*@null@*/ section *
dbg_objfmt_sections_switch(sectionhead *headp, valparamhead *valparams,
			   /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    valparam *vp;
    section *retval;
    int isnew;

#if 0
    fprintf(stderr, "-dbg_objfmt_sections_switch():\n");
    printf(" Val/Params:\n");
    vps_foreach(vp, valparams) {
	printf("  (%s,", vp->val?vp->val:"(nil)");
	if (vp->param)
	    expr_print(vp->param);
	else
	    printf("(nil)");
	printf(")\n");
    }
    printf(" Obj Ext Val/Params:\n");
    if (!objext_valparams)
	printf("  (none)\n");
    else
	vps_foreach(vp, objext_valparams) {
	    printf("  (%s,", vp->val?vp->val:"(nil)");
	    if (vp->param)
		expr_print(vp->param);
	    else
		printf("(nil)");
	    printf(")\n");
	}
#endif
    if ((vp = vps_first(valparams)) && !vp->param && vp->val != NULL) {
	retval = sections_switch_general(headp, vp->val, NULL, 0, &isnew);
	if (isnew)
	    symrec_define_label(vp->val, retval, (bytecode *)NULL, 1);
	return retval;
    } else
	return NULL;
}

static void
dbg_objfmt_section_data_delete(/*@only@*/ void *data)
{
    xfree(data);
}

static void
dbg_objfmt_section_data_print(/*@unused@*/ void *data)
{
}

static /*@null@*/ void *
dbg_objfmt_extern_data_new(const char *name, /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    printf("extern_data_new(\"%s\")\n", name);
    return NULL;
}

static /*@null@*/ void *
dbg_objfmt_global_data_new(const char *name, /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    printf("global_data_new(\"%s\")\n", name);
    return NULL;
}

static /*@null@*/ void *
dbg_objfmt_common_data_new(const char *name, /*@only@*/ expr *size,
			   /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    printf("common_data_new(\"%s\",", name);
    expr_print(size);
    printf(")\n");
    return size;
}

static void
dbg_objfmt_declare_data_delete(/*@unused@*/ SymVisibility vis,
			       /*@unused@*/ /*@only@*/ void *data)
{
    printf("declare_data_delete()\n");
    if (vis == SYM_COMMON)
	expr_delete(data);
    else
	xfree(data);
}

static int
dbg_objfmt_directive(const char *name, valparamhead *valparams,
		     /*@null@*/ valparamhead *objext_valparams)
{
    valparam *vp;

    printf("directive(\"%s\", valparams:\n", name);
    vps_foreach(vp, valparams) {
	printf("  (%s,", vp->val?vp->val:"(nil)");
	if (vp->param)
	    expr_print(vp->param);
	else
	    printf("(nil)");
	printf(")\n");
    }
    printf(" objext_valparams:\n");
    if (!objext_valparams)
	printf("  (none)\n");
    else
	vps_foreach(vp, objext_valparams) {
	    printf("  (%s,", vp->val?vp->val:"(nil)");
	    if (vp->param)
		expr_print(vp->param);
	    else
		printf("(nil)");
	    printf(")\n");
	}

    return 0;	    /* dbg format "recognizes" all directives */
}

/* Define objfmt structure -- see objfmt.h for details */
objfmt dbg_objfmt = {
    "Trace of all info passed to object format module",
    "dbg",
    ".text",
    32,
    dbg_objfmt_sections_switch,
    dbg_objfmt_section_data_delete,
    dbg_objfmt_section_data_print,
    dbg_objfmt_extern_data_new,
    dbg_objfmt_global_data_new,
    dbg_objfmt_common_data_new,
    dbg_objfmt_declare_data_delete,
    dbg_objfmt_directive
};
