/*
 * Section utility functions
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

#ifdef STDC_HEADERS
# include <assert.h>
#endif

#include "globals.h"
#include "errwarn.h"
#include "expr.h"

#include "bytecode.h"
#include "section.h"
#include "objfmt.h"


struct section {
    /*@reldef@*/ STAILQ_ENTRY(section) link;

    enum { SECTION_GENERAL, SECTION_ABSOLUTE } type;

    union {
	/* SECTION_GENERAL data */
	struct general {
	    /*@owned@*/ char *name;	/* strdup()'ed name (given by user) */

	    /* object-format-specific data */ 
	    /*@null@*/ /*@owned@*/ void *of_data;
	} general;

	/* SECTION_ABSOLUTE data */
	/*@owned@*/ expr *start;
    } data;

    
    unsigned long opt_flags;	/* storage for optimizer flags */

    int res_only;		/* allow only resb family of bytecodes? */

    bytecodehead bc;		/* the bytecodes for the section's contents */
};

/*@-compdestroy@*/
section *
sections_initialize(sectionhead *headp)
{
    section *s;
    valparamhead vps;
    valparam *vp;

    /* Initialize linked list */
    STAILQ_INIT(headp);

    /* Add an initial "default" section */
    assert(cur_objfmt != NULL);
    vp_new(vp, xstrdup(cur_objfmt->default_section_name), NULL);
    vps_initialize(&vps);
    vps_append(&vps, vp);
    s = cur_objfmt->sections_switch(headp, &vps, NULL);
    vps_delete(&vps);

    return s;
}
/*@=compdestroy@*/

/*@-onlytrans@*/
section *
sections_switch_general(sectionhead *headp, const char *name, void *of_data,
			int res_only, int *isnew)
{
    section *s;

    /* Search through current sections to see if we already have one with
     * that name.
     */
    STAILQ_FOREACH(s, headp, link) {
	if (s->type == SECTION_GENERAL &&
	    strcmp(s->data.general.name, name) == 0) {
	    if (of_data) {
		Warning(_("segment attributes specified on redeclaration of segment: ignoring"));
		assert(cur_objfmt != NULL);
		cur_objfmt->section_data_delete(of_data);
	    }
	    *isnew = 0;
	    return s;
	}
    }

    /* No: we have to allocate and create a new one. */

    /* Okay, the name is valid; now allocate and initialize */
    s = xcalloc(1, sizeof(section));
    STAILQ_INSERT_TAIL(headp, s, link);

    s->type = SECTION_GENERAL;
    s->data.general.name = xstrdup(name);
    s->data.general.of_data = of_data;
    bytecodes_initialize(&s->bc);

    s->opt_flags = 0;
    s->res_only = res_only;

    *isnew = 1;
    return s;
}
/*@=onlytrans@*/

/*@-onlytrans@*/
section *
sections_switch_absolute(sectionhead *headp, expr *start)
{
    section *s;

    s = xcalloc(1, sizeof(section));
    STAILQ_INSERT_TAIL(headp, s, link);

    s->type = SECTION_ABSOLUTE;
    s->data.start = start;
    bytecodes_initialize(&s->bc);

    s->opt_flags = 0;
    s->res_only = 1;

    return s;
}
/*@=onlytrans@*/

int
section_is_absolute(section *sect)
{
    return (sect->type == SECTION_ABSOLUTE);
}

unsigned long
section_get_opt_flags(const section *sect)
{
    return sect->opt_flags;
}

void
section_set_opt_flags(section *sect, unsigned long opt_flags)
{
    sect->opt_flags = opt_flags;
}

void
sections_delete(sectionhead *headp)
{
    section *cur, *next;

    cur = STAILQ_FIRST(headp);
    while (cur) {
	next = STAILQ_NEXT(cur, link);
	section_delete(cur);
	cur = next;
    }
    STAILQ_INIT(headp);
}

void
sections_print(FILE *f, const sectionhead *headp)
{
    section *cur;
    
    STAILQ_FOREACH(cur, headp, link) {
	fprintf(f, "%*sSection:\n", indent_level, "");
	indent_level++;
	section_print(f, cur, 1);
	indent_level--;
    }
}

void
sections_parser_finalize(sectionhead *headp)
{
    section *cur;
    
    STAILQ_FOREACH(cur, headp, link)
	bcs_parser_finalize(&cur->bc);
}

bytecodehead *
section_get_bytecodes(section *sect)
{
    return &sect->bc;
}

const char *
section_get_name(const section *sect)
{
    if (sect->type == SECTION_GENERAL)
	return sect->data.general.name;
    return NULL;
}

const expr *
section_get_start(const section *sect)
{
    if (sect->type == SECTION_ABSOLUTE)
	return sect->data.start;
    return NULL;
}

void
section_delete(section *sect)
{
    if (!sect)
	return;

    switch (sect->type) {
	case SECTION_GENERAL:
	    xfree(sect->data.general.name);
	    assert(cur_objfmt != NULL);
	    if (sect->data.general.of_data)
		cur_objfmt->section_data_delete(sect->data.general.of_data);
	    break;
	case SECTION_ABSOLUTE:
	    expr_delete(sect->data.start);
	    break;
    }
    bcs_delete(&sect->bc);
    xfree(sect);
}

void
section_print(FILE *f, const section *sect, int print_bcs)
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
	    assert(cur_objfmt != NULL);
	    indent_level++;
	    if (sect->data.general.of_data)
		cur_objfmt->section_data_print(f, sect->data.general.of_data);
	    else
		fprintf(f, "%*s(none)\n", indent_level, "");
	    indent_level--;
	    break;
	case SECTION_ABSOLUTE:
	    fprintf(f, "absolute\n%*sstart=", indent_level, "");
	    expr_print(f, sect->data.start);
	    fprintf(f, "\n");
	    break;
    }

    if (print_bcs) {
	fprintf(f, "%*sBytecodes:\n", indent_level, "");
	indent_level++;
	bcs_print(f, &sect->bc);
	indent_level--;
    }
}
