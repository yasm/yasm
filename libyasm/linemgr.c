/*
 * YASM assembler line manager (for parse stage)
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
#define YASM_LIB_INTERNAL
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "coretype.h"
#include "hamt.h"

#include "errwarn.h"
#include "linemgr.h"


/* Source lines tracking */
typedef struct {
    struct line_mapping *vector;
    unsigned long size;
    unsigned long allocated;
} line_mapping_head;

typedef struct line_mapping {
    /* monotonically increasing virtual line */
    unsigned long line;

    /* related info */
    /* "original" source filename */
    /*@null@*/ /*@dependent@*/ const char *filename;
    /* "original" source base line number */
    unsigned long file_line;
    /* "original" source line number increment (for following lines) */
    unsigned long line_inc;
} line_mapping;

typedef struct line_assoc_data_raw_head {
    /*@only@*/ void **vector;
    const yasm_assoc_data_callback *callback;
    size_t size;
} line_assoc_data_raw_head;

typedef struct line_assoc_data {
    /*@only@*/ void *data;
    const yasm_assoc_data_callback *callback;
} line_assoc_data;

struct yasm_linemap {
    /* Shared storage for filenames */
    /*@only@*/ /*@null@*/ HAMT *filenames;

    /* Current virtual line number. */
    unsigned long current;

    /* Mappings from virtual to physical line numbers */
    /*@only@*/ /*@null@*/ line_mapping_head *map;

    /* Associated data arrays for odd data types (those likely to have data
     * associated for every line).
     */
    /*@null@*/ /*@only@*/ line_assoc_data_raw_head *assoc_data_array;
    size_t assoc_data_array_size;
    size_t assoc_data_array_alloc;
};

static void
filename_delete_one(/*@only@*/ void *d)
{
    yasm_xfree(d);
}

void
yasm_linemap_set(yasm_linemap *linemap, const char *filename,
		 unsigned long file_line, unsigned long line_inc)
{
    char *copy;
    int replace = 0;
    line_mapping *mapping;

    /* Create a new mapping in the map */
    if (linemap->map->size >= linemap->map->allocated) {
	/* allocate another size bins when full for 2x space */
	linemap->map->vector =
	    yasm_xrealloc(linemap->map->vector, 2*linemap->map->allocated
			  *sizeof(line_mapping));
	linemap->map->allocated *= 2;
    }
    mapping = &linemap->map->vector[linemap->map->size];
    linemap->map->size++;

    /* Fill it */

    /* Copy the filename (via shared storage) */
    copy = yasm__xstrdup(filename);
    /*@-aliasunique@*/
    mapping->filename = HAMT_insert(linemap->filenames, copy, copy, &replace,
				    filename_delete_one);
    /*@=aliasunique@*/

    mapping->line = linemap->current;
    mapping->file_line = file_line;
    mapping->line_inc = line_inc;
}

yasm_linemap *
yasm_linemap_create(void)
{
    yasm_linemap *linemap = yasm_xmalloc(sizeof(yasm_linemap));

    linemap->filenames = HAMT_create(yasm_internal_error_);

    linemap->current = 1;

    /* initialize mapping vector */
    linemap->map = yasm_xmalloc(sizeof(line_mapping_head));
    linemap->map->vector = yasm_xmalloc(8*sizeof(line_mapping));
    linemap->map->size = 0;
    linemap->map->allocated = 8;
    
    /* initialize associated data arrays */
    linemap->assoc_data_array_size = 0;
    linemap->assoc_data_array_alloc = 2;
    linemap->assoc_data_array = yasm_xmalloc(linemap->assoc_data_array_alloc *
					     sizeof(line_assoc_data_raw_head));

    return linemap;
}

void
yasm_linemap_destroy(yasm_linemap *linemap)
{
    if (linemap->assoc_data_array) {
	size_t i;
	for (i=0; i<linemap->assoc_data_array_size; i++) {
	    line_assoc_data_raw_head *adrh = &linemap->assoc_data_array[i];
	    if (adrh->vector) {
		size_t j;
		for (j=0; j<adrh->size; j++) {
		    if (adrh->vector[j])
			adrh->callback->destroy(adrh->vector[j]);
		}
		yasm_xfree(adrh->vector);
	    }
	}
	yasm_xfree(linemap->assoc_data_array);
    }

    if (linemap->map) {
	yasm_xfree(linemap->map->vector);
	yasm_xfree(linemap->map);
    }

    if (linemap->filenames)
	HAMT_destroy(linemap->filenames, filename_delete_one);

    yasm_xfree(linemap);
}

unsigned long
yasm_linemap_get_current(yasm_linemap *linemap)
{
    return linemap->current;
}

void
yasm_linemap_add_data(yasm_linemap *linemap,
		      const yasm_assoc_data_callback *callback, void *data,
		      /*@unused@*/ int every_hint)
{
    /* FIXME: Hint not supported yet. */

    line_assoc_data_raw_head *adrh = NULL;
    size_t i;

    /* See if there's already associated data for this callback */
    for (i=0; !adrh && i<linemap->assoc_data_array_size; i++) {
	if (linemap->assoc_data_array[i].callback == callback)
	    adrh = &linemap->assoc_data_array[i];
    }

    /* No?  Then append a new one */
    if (!adrh) {
	linemap->assoc_data_array_size++;
	if (linemap->assoc_data_array_size > linemap->assoc_data_array_alloc) {
	    linemap->assoc_data_array_alloc *= 2;
	    linemap->assoc_data_array =
		yasm_xrealloc(linemap->assoc_data_array,
			      linemap->assoc_data_array_alloc *
			      sizeof(line_assoc_data_raw_head));
	}
	adrh = &linemap->assoc_data_array[linemap->assoc_data_array_size-1];

	adrh->size = 4;
	adrh->vector = yasm_xmalloc(adrh->size*sizeof(void *));
	adrh->callback = callback;
	for (i=0; i<adrh->size; i++)
	    adrh->vector[i] = NULL;
    }

    while (linemap->current > adrh->size) {
	/* allocate another size bins when full for 2x space */
	adrh->vector = yasm_xrealloc(adrh->vector,
				     2*adrh->size*sizeof(void *));
	for (i=adrh->size; i<adrh->size*2; i++)
	    adrh->vector[i] = NULL;
	adrh->size *= 2;
    }

    /* Delete existing data for that line (if any) */
    if (adrh->vector[linemap->current-1])
	adrh->callback->destroy(adrh->vector[linemap->current-1]);

    adrh->vector[linemap->current-1] = data;
}

unsigned long
yasm_linemap_goto_next(yasm_linemap *linemap)
{
    return ++(linemap->current);
}

void
yasm_linemap_lookup(yasm_linemap *linemap, unsigned long line,
		    const char **filename, unsigned long *file_line)
{
    line_mapping *mapping;
    unsigned long vindex, step;

    assert(line <= linemap->current);

    /* Binary search through map to find highest line_index <= index */
    assert(linemap->map != NULL);
    vindex = 0;
    /* start step as the greatest power of 2 <= size */
    step = 1;
    while (step*2<=linemap->map->size)
	step*=2;
    while (step>0) {
	if (vindex+step < linemap->map->size
		&& linemap->map->vector[vindex+step].line <= line)
	    vindex += step;
	step /= 2;
    }
    mapping = &linemap->map->vector[vindex];

    *filename = mapping->filename;
    *file_line = mapping->file_line + mapping->line_inc*(line-mapping->line);
}

void *
yasm_linemap_get_data(yasm_linemap *linemap, unsigned long line,
		      const yasm_assoc_data_callback *callback)
{
    line_assoc_data_raw_head *adrh = NULL;
    size_t i;

    /* Linear search through data array */
    for (i=0; !adrh && i<linemap->assoc_data_array_size; i++) {
	if (linemap->assoc_data_array[i].callback == callback)
	    adrh = &linemap->assoc_data_array[i];
    }

    if (!adrh)
	return NULL;
    
    if (line > adrh->size)
	return NULL;

    return adrh->vector[line-1];
}
