/*
 * YASM module loader
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

#include "ltdl.h"

#include "module.h"
#include "globals.h"


typedef struct module {
    SLIST_ENTRY(module) link;
    /*@only@*/ char *keyword;	    /* module keyword */
    lt_dlhandle handle;		    /* dlopen handle */
} module;

SLIST_HEAD(modulehead, module) modules = SLIST_HEAD_INITIALIZER(modules);


static /*@dependent@*/ /*@null@*/ module *
load_module(const char *keyword)
{
    module *m;
    char *name;
    lt_dlhandle handle;

    /* See if the module has already been loaded. */
    SLIST_FOREACH(m, &modules, link) {
	if (strcasecmp(m->keyword, keyword) == 0)
	    return m;
    }

    /* Look for dynamic module.  First build full module name from keyword. */
    name = xmalloc(5+strlen(keyword)+1);
    strcpy(name, "yasm-");
    strcat(name, keyword);
    handle = lt_dlopenext(name);

    if (!handle) {
	xfree(name);
	return NULL;
    }

    m = xmalloc(sizeof(module));
    m->keyword = name;
    strcpy(m->keyword, keyword);
    m->handle = handle;
    SLIST_INSERT_HEAD(&modules, m, link);
    return m;
}

void
unload_modules(void)
{
    module *m;

    while (!SLIST_EMPTY(&modules)) {
	m = SLIST_FIRST(&modules);
	SLIST_REMOVE_HEAD(&modules, link);
	xfree(m->keyword);
	lt_dlclose(m->handle);
	xfree(m);
    }
}

void *
get_module_data(const char *keyword, const char *symbol)
{
    /*@dependent@*/ module *m;

    /* Load module */
    m = load_module(keyword);
    if (!m)
	return NULL;

    /* Find and return data pointer: NULL if it doesn't exist */
    return lt_dlsym(m->handle, symbol);
}
