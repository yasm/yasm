/* $IdPath$
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "util.h"

#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif

#include <libintl.h>
#define	_(String)	gettext(String)
#ifdef gettext_noop
#define	N_(String)	gettext_noop(String)
#else
#define	N_(String)	(String)
#endif

#include "globals.h"
#include "errwarn.h"
#include "expr.h"

#include "bytecode.h"
#include "section.h"
#include "objfmt.h"

RCSID("$IdPath$");

section *
sections_initialize(sectionhead *headp, objfmt *of)
{
    section *s;

    /* Initialize linked list */
    STAILQ_INIT(headp);

    /* Add an initial "default" section to the list */
    s = calloc(1, sizeof(section));
    if (!s)
	Fatal(FATAL_NOMEM);
    STAILQ_INSERT_TAIL(headp, s, link);

    /* Initialize default section */
    s->type = SECTION_GENERAL;
    s->name = strdup(of->default_section_name);
    bytecodes_initialize(&s->bc);

    return s;
}

section *
sections_switch(sectionhead *headp, objfmt *of, const char *name)
{
    section *s, *sp;

    /* Search through current sections to see if we already have one with
     * that name.
     */
    s = (section *)NULL;
    STAILQ_FOREACH(sp, headp, link) {
	if (strcmp(sp->name, name) == 0)
	    s = sp;
    }

    if (s)
	return s;

    /* No: we have to allocate and create a new one. */

    /* But first check with objfmt to see if the name is valid.
     * If it isn't, error and just return the default (first) section.
     */
    if (!of->is_valid_section(name)) {
	Error(_("Invalid section name: %s"), name);
	return STAILQ_FIRST(headp);
    }

    /* Okay, the name is valid; now allocate and initialize */
    s = calloc(1, sizeof(section));
    if (!s)
	Fatal(FATAL_NOMEM);
    STAILQ_INSERT_TAIL(headp, s, link);

    s->type = SECTION_GENERAL;
    s->name = strdup(name);
    bytecodes_initialize(&s->bc);

    return s;
}
