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


    int res_only;		/* allow only resb family of bytecodes? */

    bytecodehead bc;		/* the bytecodes for the section's contents */
};

section *
sections_initialize(sectionhead *headp)
{
    section *s;

    /* Initialize linked list */
    STAILQ_INIT(headp);

    /* Add an initial "default" section to the list */
    s = xcalloc(1, sizeof(section));
    STAILQ_INSERT_TAIL(headp, s, link);

    /* Initialize default section */
    s->type = SECTION_GENERAL;
    assert(cur_objfmt != NULL);
    s->data.general.name = xstrdup(cur_objfmt->default_section_name);
    s->data.general.of_data = NULL;
    bytecodes_initialize(&s->bc);

    s->res_only = 0;

    return s;
}

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

    s->res_only = 1;

    return s;
}
/*@=onlytrans@*/

int
section_is_absolute(section *sect)
{
    return (sect->type == SECTION_ABSOLUTE);
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
sections_print(const sectionhead *headp)
{
    section *cur;
    
    STAILQ_FOREACH(cur, headp, link) {
	printf("***SECTION***\n");
	section_print(cur, 1);
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
section_print(const section *sect, int print_bcs)
{
    printf(" type=");
    switch (sect->type) {
	case SECTION_GENERAL:
	    printf("general\n name=%s\n objfmt data:\n",
		   sect->data.general.name);
	    assert(cur_objfmt != NULL);
	    if (sect->data.general.of_data)
		cur_objfmt->section_data_print(sect->data.general.of_data);
	    else
		printf("  (none)\n");
	    break;
	case SECTION_ABSOLUTE:
	    printf("absolute\n start=");
	    expr_print(sect->data.start);
	    printf("\n");
	    break;
    }

    if (print_bcs) {
	printf(" Bytecodes:\n");
	bcs_print(&sect->bc);
    }
}
