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

/* Shared storage for filenames */
static /*@only@*/ /*@null@*/ HAMT *filename_table;

/* Virtual line number.  Uniquely specifies every line read by the parser. */
static unsigned long line_index;
static /*@only@*/ /*@null@*/ line_index_mapping_head *line_index_map;

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
    filename_table = HAMT_new(error_func);

    line_index = 1;

    /* initialize mapping vector */
    line_index_map = xmalloc(sizeof(line_index_mapping_head));
    line_index_map->vector = xmalloc(8*sizeof(line_index_mapping));
    line_index_map->size = 0;
    line_index_map->allocated = 8;
}

static void
yasm_linemgr_cleanup(void)
{
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

linemgr yasm_linemgr = {
    yasm_linemgr_initialize,
    yasm_linemgr_cleanup,
    yasm_linemgr_get_current,
    yasm_linemgr_goto_next,
    yasm_linemgr_set,
    yasm_linemgr_lookup
};
