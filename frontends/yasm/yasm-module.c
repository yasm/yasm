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
#include <util.h>
/*@unused@*/ RCSID("$IdPath$");

#include <libyasm/compat-queue.h>
#include <libyasm.h>

#include "ltdl.h"

#include "yasm-module.h"


extern const lt_dlsymlist lt_preloaded_symbols[];

typedef struct module {
    SLIST_ENTRY(module) link;
    module_type type;		    /* module type */
    /*@only@*/ char *type_str;
    char *keyword;		    /* module keyword */
    lt_dlhandle handle;		    /* dlopen handle */
} module;

static SLIST_HEAD(modulehead, module) modules = SLIST_HEAD_INITIALIZER(modules);

static const char *module_type_str[] = {
    "arch",
    "dbgfmt",
    "objfmt",
    "optimizer",
    "parser",
    "preproc"
};

static /*@dependent@*/ /*@null@*/ module *
load_module(module_type type, const char *keyword)
{
    module *m;
    char *name;
    lt_dlhandle handle;
    size_t typelen;

    /* See if the module has already been loaded. */
    SLIST_FOREACH(m, &modules, link) {
	if (m->type == type && yasm__strcasecmp(m->keyword, keyword) == 0)
	    return m;
    }

    /* Look for dynamic module.  First build full module name from keyword. */
    typelen = strlen(module_type_str[type]);
    name = yasm_xmalloc(typelen+strlen(keyword)+2);
    strcpy(name, module_type_str[type]);
    strcat(name, "_");
    strcat(name, keyword);
    handle = lt_dlopenext(name);

    if (!handle) {
	yasm_xfree(name);
	return NULL;
    }

    m = yasm_xmalloc(sizeof(module));
    m->type = type;
    m->type_str = name;
    name[typelen] = '\0';
    m->keyword = &name[typelen+1];
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
	yasm_xfree(m->type_str);
	lt_dlclose(m->handle);
	yasm_xfree(m);
    }
}

void *
get_module_data(module_type type, const char *keyword, const char *symbol)
{
    char *name;
    /*@dependent@*/ module *m;
    void *data;

    /* Load module */
    m = load_module(type, keyword);
    if (!m)
	return NULL;

    name = yasm_xmalloc(strlen(keyword)+strlen(symbol)+11);

    strcpy(name, "yasm_");
    strcat(name, keyword);
    strcat(name, "_LTX_");
    strcat(name, symbol);

    /* Find and return data pointer: NULL if it doesn't exist */
    data = lt_dlsym(m->handle, name);

    yasm_xfree(name);
    return data;
}

typedef struct list_module_info {
    SLIST_ENTRY(list_module_info) link;
    char keyword[20];
    char name[60];
} list_module_info;

typedef struct list_module_data {
    module_type type;
    list_module_info *matches;
    size_t matches_num, matches_alloc;
} list_module_data;

#ifdef HAVE_BASENAME
extern char *basename(const char *);
#else
static const char *
basename(const char *path)
{
    const char *base;
    base = strrchr(path, '/');
    if (!base)
	base = path;
    else
	base++;
}
#endif

static int
list_module_load(const char *filename, lt_ptr data)
{
    list_module_data *lmdata = data;
    const char *base;
    const char *module_keyword = NULL, *module_name = NULL;
    char *name;

    yasm_arch *arch;
    yasm_dbgfmt *dbgfmt;
    yasm_objfmt *objfmt;
    yasm_optimizer *optimizer;
    yasm_parser *parser;
    yasm_preproc *preproc;

    lt_dlhandle handle;

    /* Strip off path components, if any */
    base = basename(filename);
    if (!base)
	return 0;

    /* All modules have '_' in them; early check */
    if (!strchr(base, '_'))
	return 0;

    /* Check to see if module is of the type we're looking for.
     * Even though this check is also implicitly performed below, there's a
     * massive speedup in avoiding the dlopen() call.
     */
    if (strncmp(base, module_type_str[lmdata->type],
		strlen(module_type_str[lmdata->type])) != 0)
	return 0;

    /* Load it */
    handle = lt_dlopenext(filename);
    if (!handle)
	return 0;

    /* Build base symbol name */
    name = yasm_xmalloc(strlen(base)+5+
			strlen(module_type_str[lmdata->type])+1);
    strcpy(name, base);
    strcat(name, "_LTX_");
    strcat(name, module_type_str[lmdata->type]);

    /* Prefix with yasm in the right place and get name/keyword */
    switch (lmdata->type) {
	case MODULE_ARCH:
	    strncpy(name, "yasm", 4);
	    arch = lt_dlsym(handle, name);
	    if (arch) {
		module_keyword = arch->keyword;
		module_name = arch->name;
	    }
	    break;
	case MODULE_DBGFMT:
	    strncpy(name+2, "yasm", 4);
	    dbgfmt = lt_dlsym(handle, name+2);
	    if (dbgfmt) {
		module_keyword = dbgfmt->keyword;
		module_name = dbgfmt->name;
	    }
	    break;
	case MODULE_OBJFMT:
	    strncpy(name+2, "yasm", 4);
	    objfmt = lt_dlsym(handle, name+2);
	    if (objfmt) {
		module_keyword = objfmt->keyword;
		module_name = objfmt->name;
	    }
	    break;
	case MODULE_OPTIMIZER:
	    strncpy(name+5, "yasm", 4);
	    optimizer = lt_dlsym(handle, name+5);
	    if (optimizer) {
		module_keyword = optimizer->keyword;
		module_name = optimizer->name;
	    }
	    break;
	case MODULE_PARSER:
	    strncpy(name+2, "yasm", 4);
	    parser = lt_dlsym(handle, name+2);
	    if (parser) {
		module_keyword = parser->keyword;
		module_name = parser->name;
	    }
	    break;
	case MODULE_PREPROC:
	    strncpy(name+3, "yasm", 4);
	    preproc = lt_dlsym(handle, name+3);
	    if (preproc) {
		module_keyword = preproc->keyword;
		module_name = preproc->name;
	    }
	    break;
    }

    if (module_keyword && module_name) {
	list_module_info *lminfo;
	/* Find empty location in array */
	if (lmdata->matches_num >= lmdata->matches_alloc) {
	    lmdata->matches_alloc *= 2;
	    lmdata->matches =
		yasm_xrealloc(lmdata->matches,
			      sizeof(list_module_info)*lmdata->matches_alloc);
	}
	lminfo = &lmdata->matches[lmdata->matches_num++];
	/* Build lminfo structure */
	strncpy(lminfo->keyword, module_keyword, sizeof(lminfo->keyword) - 1);
	lminfo->keyword[sizeof(lminfo->keyword) - 1] = '\0';
	strncpy(lminfo->name, module_name, sizeof(lminfo->name) - 1);
	lminfo->name[sizeof(lminfo->name) - 1] = '\0';
    }

    /* Clean up */
    yasm_xfree(name);
    lt_dlclose(handle);
    return 0;
}

static int
list_module_compare(const void *n1, const void *n2)
{
    const list_module_info *i1 = n1, *i2 = n2;
    return strcmp(i1->keyword, i2->keyword);
}

void
list_modules(module_type type,
	     void (*printfunc) (const char *name, const char *keyword))
{
    size_t i;
    const lt_dlsymlist *preloaded;
    char name[100];
    char *dot;
    list_module_data lmdata;
    char *prev_keyword = NULL;

    lmdata.type = type;
    lmdata.matches_num = 0;
    lmdata.matches_alloc = 10;
    lmdata.matches =
	yasm_xmalloc(sizeof(list_module_info)*lmdata.matches_alloc);

    /* Search preloaded symbols */
    preloaded = lt_preloaded_symbols;
    while (preloaded->name) {
	/* Strip out any library extension */
	strncpy(name, preloaded->name, sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';
	dot = strrchr(name, '.');
	if (dot)
	    *dot = '\0';

	/* Search it */
	list_module_load(name, &lmdata);
	preloaded++;
    }
    /* Search external module path */
    lt_dlforeachfile(NULL, list_module_load, &lmdata);
    lt_dlforeachfile(".", list_module_load, &lmdata);

    /* Sort, print, and cleanup */
    yasm__mergesort(lmdata.matches, lmdata.matches_num,
		    sizeof(list_module_info), list_module_compare);
    for (i=0; i<lmdata.matches_num; i++) {
	/* Don't print duplicates */
	if (!prev_keyword || strcmp(prev_keyword, lmdata.matches[i].keyword))
	    printfunc(lmdata.matches[i].name, lmdata.matches[i].keyword);
	prev_keyword = lmdata.matches[i].keyword;
    }
    yasm_xfree(lmdata.matches);
}
