/*
 * YASM module loader
 *
 *  Copyright (C) 2002  Peter Johnson
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

#include "ltdl.h"

#include "module.h"


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
