/*
 * Section utility functions
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
/*@unused@*/ RCSID("$IdPath$");

#include "errwarn.h"
#include "intnum.h"
#include "expr.h"

#include "bytecode.h"
#include "section.h"
#include "objfmt.h"


struct yasm_section {
    /*@reldef@*/ STAILQ_ENTRY(yasm_section) link;

    enum { SECTION_GENERAL, SECTION_ABSOLUTE } type;

    union {
	/* SECTION_GENERAL data */
	struct general {
	    /*@owned@*/ char *name;	/* strdup()'ed name (given by user) */

	    /* object-format-specific data */
	    /*@null@*/ /*@dependent@*/ yasm_objfmt *of;
	    /*@null@*/ /*@owned@*/ void *of_data;
	} general;
    } data;

    /*@owned@*/ yasm_expr *start;   /* Starting address of section contents */

    unsigned long opt_flags;	/* storage for optimizer flags */

    int res_only;		/* allow only resb family of bytecodes? */

    yasm_bytecodehead bc;	/* the bytecodes for the section's contents */

    /*@exits@*/ void (*error_func) (const char *file, unsigned int line,
				    const char *message);
};

/*@-compdestroy@*/
yasm_section *
yasm_sections_initialize(yasm_sectionhead *headp, yasm_objfmt *of)
{
    yasm_section *s;
    yasm_valparamhead vps;
    yasm_valparam *vp;

    /* Initialize linked list */
    STAILQ_INIT(headp);

    /* Add an initial "default" section */
    yasm_vp_new(vp, xstrdup(of->default_section_name), NULL);
    yasm_vps_initialize(&vps);
    yasm_vps_append(&vps, vp);
    s = of->sections_switch(headp, &vps, NULL, 0);
    yasm_vps_delete(&vps);

    return s;
}
/*@=compdestroy@*/

/*@-onlytrans@*/
yasm_section *
yasm_sections_switch_general(yasm_sectionhead *headp, const char *name,
			     unsigned long start, int res_only, int *isnew,
			     unsigned long lindex,
			     /*@exits@*/ void (*error_func)
				(const char *file, unsigned int line,
				 const char *message))
{
    yasm_section *s;

    /* Search through current sections to see if we already have one with
     * that name.
     */
    STAILQ_FOREACH(s, headp, link) {
	if (s->type == SECTION_GENERAL &&
	    strcmp(s->data.general.name, name) == 0) {
	    *isnew = 0;
	    return s;
	}
    }

    /* No: we have to allocate and create a new one. */

    /* Okay, the name is valid; now allocate and initialize */
    s = xcalloc(1, sizeof(yasm_section));
    STAILQ_INSERT_TAIL(headp, s, link);

    s->type = SECTION_GENERAL;
    s->data.general.name = xstrdup(name);
    s->data.general.of = NULL;
    s->data.general.of_data = NULL;
    s->start = yasm_expr_new_ident(yasm_expr_int(yasm_intnum_new_uint(start)),
				   lindex);
    yasm_bcs_initialize(&s->bc);

    s->opt_flags = 0;
    s->res_only = res_only;

    s->error_func = error_func;

    *isnew = 1;
    return s;
}
/*@=onlytrans@*/

/*@-onlytrans@*/
yasm_section *
yasm_sections_switch_absolute(yasm_sectionhead *headp, yasm_expr *start,
			      /*@exits@*/ void (*error_func)
				(const char *file, unsigned int line,
				 const char *message))
{
    yasm_section *s;

    s = xcalloc(1, sizeof(yasm_section));
    STAILQ_INSERT_TAIL(headp, s, link);

    s->type = SECTION_ABSOLUTE;
    s->start = start;
    yasm_bcs_initialize(&s->bc);

    s->opt_flags = 0;
    s->res_only = 1;

    s->error_func = error_func;

    return s;
}
/*@=onlytrans@*/

int
yasm_section_is_absolute(yasm_section *sect)
{
    return (sect->type == SECTION_ABSOLUTE);
}

unsigned long
yasm_section_get_opt_flags(const yasm_section *sect)
{
    return sect->opt_flags;
}

void
yasm_section_set_opt_flags(yasm_section *sect, unsigned long opt_flags)
{
    sect->opt_flags = opt_flags;
}

void
yasm_section_set_of_data(yasm_section *sect, yasm_objfmt *of, void *of_data)
{
    /* Check to see if section type supports of_data */
    if (sect->type != SECTION_GENERAL) {
	if (of->section_data_delete)
	    of->section_data_delete(of_data);
	else
	    sect->error_func(__FILE__, __LINE__,
		N_("don't know how to delete objfmt-specific section data"));
	return;
    }

    /* Delete current of_data if present */
    if (sect->data.general.of_data && sect->data.general.of) {
	yasm_objfmt *of2 = sect->data.general.of;
	if (of2->section_data_delete)
	    of2->section_data_delete(sect->data.general.of_data);
	else
	    sect->error_func(__FILE__, __LINE__,
		N_("don't know how to delete objfmt-specific section data"));
    }

    /* Assign new of_data */
    sect->data.general.of = of;
    sect->data.general.of_data = of_data;
}

void *
yasm_section_get_of_data(yasm_section *sect)
{
    if (sect->type == SECTION_GENERAL)
	return sect->data.general.of_data;
    else
	return NULL;
}

void
yasm_sections_delete(yasm_sectionhead *headp)
{
    yasm_section *cur, *next;

    cur = STAILQ_FIRST(headp);
    while (cur) {
	next = STAILQ_NEXT(cur, link);
	yasm_section_delete(cur);
	cur = next;
    }
    STAILQ_INIT(headp);
}

void
yasm_sections_print(FILE *f, int indent_level, const yasm_sectionhead *headp)
{
    yasm_section *cur;
    
    STAILQ_FOREACH(cur, headp, link) {
	fprintf(f, "%*sSection:\n", indent_level, "");
	yasm_section_print(f, indent_level+1, cur, 1);
    }
}

int
yasm_sections_traverse(yasm_sectionhead *headp, /*@null@*/ void *d,
		       int (*func) (yasm_section *sect, /*@null@*/ void *d))
{
    yasm_section *cur;
    
    STAILQ_FOREACH(cur, headp, link) {
	int retval = func(cur, d);
	if (retval != 0)
	    return retval;
    }
    return 0;
}

/*@-onlytrans@*/
yasm_section *
yasm_sections_find_general(yasm_sectionhead *headp, const char *name)
{
    yasm_section *cur;

    STAILQ_FOREACH(cur, headp, link) {
	if (cur->type == SECTION_GENERAL &&
	    strcmp(cur->data.general.name, name) == 0)
	    return cur;
    }
    return NULL;
}
/*@=onlytrans@*/

yasm_bytecodehead *
yasm_section_get_bytecodes(yasm_section *sect)
{
    return &sect->bc;
}

const char *
yasm_section_get_name(const yasm_section *sect)
{
    if (sect->type == SECTION_GENERAL)
	return sect->data.general.name;
    return NULL;
}

void
yasm_section_set_start(yasm_section *sect, unsigned long start,
		       unsigned long lindex)
{
    yasm_expr_delete(sect->start);
    sect->start =
	yasm_expr_new_ident(yasm_expr_int(yasm_intnum_new_uint(start)),
			    lindex);
}

const yasm_expr *
yasm_section_get_start(const yasm_section *sect)
{
    return sect->start;
}

void
yasm_section_delete(yasm_section *sect)
{
    if (!sect)
	return;

    if (sect->type == SECTION_GENERAL) {
	xfree(sect->data.general.name);
	if (sect->data.general.of_data && sect->data.general.of) {
	    yasm_objfmt *of = sect->data.general.of;
	    if (of->section_data_delete)
		of->section_data_delete(sect->data.general.of_data);
	    else
		sect->error_func(__FILE__, __LINE__,
		    N_("don't know how to delete objfmt-specific section data"));
	}
    }
    yasm_expr_delete(sect->start);
    yasm_bcs_delete(&sect->bc);
    xfree(sect);
}

void
yasm_section_print(FILE *f, int indent_level, const yasm_section *sect,
		   int print_bcs)
{
    if (!sect) {
	fprintf(f, "%*s(none)\n", indent_level, "");
	return;
    }

    fprintf(f, "%*stype=", indent_level, "");
    switch (sect->type) {
	case SECTION_GENERAL:
	    fprintf(f, "general\n%*sname=%s\n%*sobjfmt data:\n", indent_level,
		    "", sect->data.general.name, indent_level, "");
	    indent_level++;
	    if (sect->data.general.of_data && sect->data.general.of) {
		yasm_objfmt *of = sect->data.general.of;
		if (of->section_data_print)
		    of->section_data_print(f, indent_level,
					   sect->data.general.of_data);
		else
		    fprintf(f, "%*sUNKNOWN\n", indent_level, "");
	    } else
		fprintf(f, "%*s(none)\n", indent_level, "");
	    indent_level--;
	    break;
	case SECTION_ABSOLUTE:
	    fprintf(f, "absolute\n");
	    break;
    }

    fprintf(f, "%*sstart=", indent_level, "");
    yasm_expr_print(f, sect->start);
    fprintf(f, "\n");

    if (print_bcs) {
	fprintf(f, "%*sBytecodes:\n", indent_level, "");
	yasm_bcs_print(f, indent_level+1, &sect->bc);
    }
}
