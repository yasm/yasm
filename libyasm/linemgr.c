/*
 * YASM assembler line manager (for parse stage)
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

#include "hamt.h"

#include "linemgr.h"


static /*@exits@*/ void (*efunc) (const char *file, unsigned int line,
				  const char *message);

/* Source lines tracking */
typedef struct {
    struct line_index_mapping *vector;
    unsigned long size;
    unsigned long allocated;
} line_index_mapping_head;

typedef struct line_index_mapping {
    /* monotonically increasing line index */
    unsigned long index;

    /* related info */
    /* "original" source filename */
    /*@null@*/ /*@dependent@*/ const char *filename;
    /* "original" source base line number */
    unsigned long line;
    /* "original" source line number increment (for following lines) */
    unsigned long line_inc;
} line_index_mapping;

typedef struct line_index_assoc_data_raw_head {
    /*@only@*/ void **vector;
    /*@null@*/ void (*delete_func) (/*@only@*/ void *);
    unsigned long size;
} line_index_assoc_data_raw_head;

typedef struct line_index_assoc_data {
    /*@only@*/ void *data;
    /*@null@*/ void (*delete_func) (/*@only@*/ void *);
    int type;
} line_index_assoc_data;

/* Shared storage for filenames */
static /*@only@*/ /*@null@*/ HAMT *filename_table;

/* Virtual line number.  Uniquely specifies every line read by the parser. */
static unsigned long line_index;
static /*@only@*/ /*@null@*/ line_index_mapping_head *line_index_map;

/* Associated data arrays for odd data types (those likely to have data
 * associated for every line).
 */
static line_index_assoc_data_raw_head *line_index_assoc_data_array;
#define MAX_LINE_INDEX_ASSOC_DATA_ARRAY	    8

static void
filename_delete_one(/*@only@*/ void *d)
{
    xfree(d);
}

static void
yasm_linemgr_set(const char *filename, unsigned long line,
		 unsigned long line_inc)
{
    char *copy;
    int replace = 0;
    line_index_mapping *mapping;

    /* Create a new mapping in the map */
    if (line_index_map->size >= line_index_map->allocated) {
	/* allocate another size bins when full for 2x space */
	line_index_map->vector = xrealloc(line_index_map->vector,
		2*line_index_map->allocated*sizeof(line_index_mapping));
	line_index_map->allocated *= 2;
    }
    mapping = &line_index_map->vector[line_index_map->size];
    line_index_map->size++;

    /* Fill it */

    /* Copy the filename (via shared storage) */
    copy = xstrdup(filename);
    /*@-aliasunique@*/
    mapping->filename = HAMT_insert(filename_table, copy, copy, &replace,
				    filename_delete_one);
    /*@=aliasunique@*/

    mapping->index = line_index;
    mapping->line = line;
    mapping->line_inc = line_inc;
}

static void
yasm_linemgr_initialize(/*@exits@*/
			void (*error_func) (const char *file,
					    unsigned int line,
					    const char *message))
{
    int i;

    filename_table = HAMT_new(error_func);
    efunc = error_func;

    line_index = 1;

    /* initialize mapping vector */
    line_index_map = xmalloc(sizeof(line_index_mapping_head));
    line_index_map->vector = xmalloc(8*sizeof(line_index_mapping));
    line_index_map->size = 0;
    line_index_map->allocated = 8;
    
    /* initialize associated data arrays */
    line_index_assoc_data_array =
	xmalloc(MAX_LINE_INDEX_ASSOC_DATA_ARRAY *
		sizeof(line_index_assoc_data_raw_head));
    for (i=0; i<MAX_LINE_INDEX_ASSOC_DATA_ARRAY; i++) {
	line_index_assoc_data_array[i].vector = NULL;
	line_index_assoc_data_array[i].size = 0;
    }
}

static void
yasm_linemgr_cleanup(void)
{
    if (line_index_assoc_data_array) {
	int i;
	for (i=0; i<MAX_LINE_INDEX_ASSOC_DATA_ARRAY; i++) {
	    line_index_assoc_data_raw_head *adrh =
		&line_index_assoc_data_array[i];
	    if (adrh->delete_func && adrh->vector) {
		int j;
		for (j=0; j<adrh->size; j++) {
		    if (adrh->vector[j])
			adrh->delete_func(adrh->vector[j]);
		}
		xfree(adrh->vector);
	    }
	}
	xfree(line_index_assoc_data_array);
	line_index_assoc_data_array = NULL;
    }

    if (line_index_map) {
	xfree(line_index_map->vector);
	xfree(line_index_map);
	line_index_map = NULL;
    }

    if (filename_table) {
	HAMT_delete(filename_table, filename_delete_one);
	filename_table = NULL;
    }
}

static unsigned long
yasm_linemgr_get_current(void)
{
    return line_index;
}

static void
yasm_linemgr_add_assoc_data(int type, /*@only@*/ void *data,
			    /*@null@*/ void (*delete_func) (void *))
{
    if ((type & 1) && type < MAX_LINE_INDEX_ASSOC_DATA_ARRAY*2) {
	line_index_assoc_data_raw_head *adrh =
	    &line_index_assoc_data_array[type>>1];

	if (adrh->size == 0) {
	    int i;

	    adrh->size = 4;
	    adrh->vector = xmalloc(adrh->size*sizeof(void *));
	    adrh->delete_func = delete_func;
	    for (i=0; i<adrh->size; i++)
		adrh->vector[i] = NULL;
	}

	while (line_index > adrh->size) {
	    int i;

	    /* allocate another size bins when full for 2x space */
	    adrh->vector = xrealloc(adrh->vector, 2*adrh->size*sizeof(void *));
	    for(i=adrh->size; i<adrh->size*2; i++)
		adrh->vector[i] = NULL;
	    adrh->size *= 2;
	}

	adrh->vector[line_index-1] = data;
	if (adrh->delete_func != delete_func)
	    efunc(__FILE__, __LINE__,
		  N_("multiple deletion functions specified"));
    } else {
	efunc(__FILE__, __LINE__, N_("non-common data not supported yet"));
	delete_func(data);
    }
}

static unsigned long
yasm_linemgr_goto_next(void)
{
    return ++line_index;
}

static void
yasm_linemgr_lookup(unsigned long lindex, const char **filename,
		    unsigned long *line)
{
    line_index_mapping *mapping;
    unsigned long vindex, step;

    assert(lindex <= line_index);

    /* Binary search through map to find highest line_index <= index */
    assert(line_index_map != NULL);
    vindex = 0;
    /* start step as the greatest power of 2 <= size */
    step = 1;
    while (step*2<=line_index_map->size)
	step*=2;
    while (step>0) {
	if (vindex+step < line_index_map->size
		&& line_index_map->vector[vindex+step].index <= lindex)
	    vindex += step;
	step /= 2;
    }
    mapping = &line_index_map->vector[vindex];

    *filename = mapping->filename;
    *line = mapping->line+mapping->line_inc*(lindex-mapping->index);
}

static /*@dependent@*/ /*@null@*/ void *
yasm_linemgr_lookup_data(unsigned long lindex, int type)
{
    if ((type & 1) && type < MAX_LINE_INDEX_ASSOC_DATA_ARRAY*2) {
	line_index_assoc_data_raw_head *adrh =
	    &line_index_assoc_data_array[type>>1];
	
	if (lindex > adrh->size)
	    return NULL;
	return adrh->vector[lindex-1];
    } else
	return NULL;
}

linemgr yasm_linemgr = {
    yasm_linemgr_initialize,
    yasm_linemgr_cleanup,
    yasm_linemgr_get_current,
    yasm_linemgr_add_assoc_data,
    yasm_linemgr_goto_next,
    yasm_linemgr_set,
    yasm_linemgr_lookup,
    yasm_linemgr_lookup_data
};
