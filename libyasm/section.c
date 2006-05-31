/*
 * Section utility functions
 *
 *  Copyright (C) 2001-2005  Peter Johnson
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
/*@unused@*/ RCSID("$Id$");

#include <limits.h>

#include "coretype.h"
#include "valparam.h"
#include "assocdat.h"
#include "qq.h"

#include "linemgr.h"
#include "errwarn.h"
#include "intnum.h"
#include "expr.h"
#include "symrec.h"

#include "bytecode.h"
#include "arch.h"
#include "section.h"
#include "objfmt.h"

#include "bc-int.h"


struct yasm_object {
    /*@owned@*/ char *src_filename;
    /*@owned@*/ char *obj_filename;

    yasm_symtab	*symtab;
    yasm_linemap *linemap;

    /*@reldef@*/ STAILQ_HEAD(yasm_sectionhead, yasm_section) sections;
};

struct yasm_section {
    /*@reldef@*/ STAILQ_ENTRY(yasm_section) link;

    /*@dependent@*/ yasm_object *object;    /* Pointer to parent object */

    enum { SECTION_GENERAL, SECTION_ABSOLUTE } type;

    union {
	/* SECTION_GENERAL data */
	struct {
	    /*@owned@*/ char *name;	/* strdup()'ed name (given by user) */
	} general;
	/* SECTION_ABSOLUTE data */
	struct {
	    /* Internally created first symrec in section.  Used by
	     * yasm_expr__level_tree during absolute reference expansion.
	     */
	    /*@dependent@*/ yasm_symrec *first;
	} absolute;
    } data;

    /* associated data; NULL if none */
    /*@null@*/ /*@only@*/ yasm__assoc_data *assoc_data;

    /*@owned@*/ yasm_expr *start;   /* Starting address of section contents */

    unsigned long align;	/* Section alignment */

    unsigned long opt_flags;	/* storage for optimizer flags */

    int code;			/* section contains code (instructions) */
    int res_only;		/* allow only resb family of bytecodes? */
    int def;			/* "default" section, e.g. not specified by
				   using section directive */

    /* the bytecodes for the section's contents */
    /*@reldef@*/ STAILQ_HEAD(yasm_bytecodehead, yasm_bytecode) bcs;

    /* the relocations for the section */
    /*@reldef@*/ STAILQ_HEAD(yasm_relochead, yasm_reloc) relocs;

    void (*destroy_reloc) (/*@only@*/ void *reloc);
};

static void yasm_section_destroy(/*@only@*/ yasm_section *sect);


/*@-compdestroy@*/
yasm_object *
yasm_object_create(const char *src_filename, const char *obj_filename)
{
    yasm_object *object = yasm_xmalloc(sizeof(yasm_object));

    object->src_filename = yasm__xstrdup(src_filename);
    object->obj_filename = yasm__xstrdup(obj_filename);

    /* Create empty symtab and linemap */
    object->symtab = yasm_symtab_create();
    object->linemap = yasm_linemap_create();

    /* Initialize sections linked list */
    STAILQ_INIT(&object->sections);

    return object;
}
/*@=compdestroy@*/

/*@-onlytrans@*/
yasm_section *
yasm_object_get_general(yasm_object *object, const char *name,
			yasm_expr *start, unsigned long align, int code,
			int res_only, int *isnew, unsigned long line)
{
    yasm_section *s;
    yasm_bytecode *bc;

    /* Search through current sections to see if we already have one with
     * that name.
     */
    STAILQ_FOREACH(s, &object->sections, link) {
	if (s->type == SECTION_GENERAL &&
	    strcmp(s->data.general.name, name) == 0) {
	    *isnew = 0;
	    return s;
	}
    }

    /* No: we have to allocate and create a new one. */

    /* Okay, the name is valid; now allocate and initialize */
    s = yasm_xcalloc(1, sizeof(yasm_section));
    STAILQ_INSERT_TAIL(&object->sections, s, link);

    s->object = object;
    s->type = SECTION_GENERAL;
    s->data.general.name = yasm__xstrdup(name);
    s->assoc_data = NULL;
    if (start)
	s->start = start;
    else
	s->start =
	    yasm_expr_create_ident(yasm_expr_int(yasm_intnum_create_uint(0)),
				   line);
    s->align = align;

    /* Initialize bytecodes with one empty bytecode (acts as "prior" for first
     * real bytecode in section.
     */
    STAILQ_INIT(&s->bcs);
    bc = yasm_bc_create_common(NULL, NULL, 0);
    bc->section = s;
    STAILQ_INSERT_TAIL(&s->bcs, bc, link);

    /* Initialize relocs */
    STAILQ_INIT(&s->relocs);
    s->destroy_reloc = NULL;

    s->code = code;
    s->res_only = res_only;
    s->def = 0;

    *isnew = 1;
    return s;
}
/*@=onlytrans@*/

/*@-onlytrans@*/
yasm_section *
yasm_object_create_absolute(yasm_object *object, yasm_expr *start,
			    unsigned long line)
{
    yasm_section *s;
    yasm_bytecode *bc;

    s = yasm_xcalloc(1, sizeof(yasm_section));
    STAILQ_INSERT_TAIL(&object->sections, s, link);

    s->object = object;
    s->type = SECTION_ABSOLUTE;
    s->start = start;

    /* Initialize bytecodes with one empty bytecode (acts as "prior" for first
     * real bytecode in section.
     */
    STAILQ_INIT(&s->bcs);
    bc = yasm_bc_create_common(NULL, NULL, 0);
    bc->section = s;
    STAILQ_INSERT_TAIL(&s->bcs, bc, link);

    /* Initialize relocs */
    STAILQ_INIT(&s->relocs);
    s->destroy_reloc = NULL;

    s->data.absolute.first =
	yasm_symtab_define_label(object->symtab, ".absstart", bc, 0, 0);

    s->code = 0;
    s->res_only = 1;
    s->def = 0;

    return s;
}
/*@=onlytrans@*/

void
yasm_object_set_source_fn(yasm_object *object, const char *src_filename)
{
    yasm_xfree(object->src_filename);
    object->src_filename = yasm__xstrdup(src_filename);
}

const char *
yasm_object_get_source_fn(const yasm_object *object)
{
    return object->src_filename;
}

const char *
yasm_object_get_object_fn(const yasm_object *object)
{
    return object->obj_filename;
}

yasm_symtab *
yasm_object_get_symtab(const yasm_object *object)
{
    return object->symtab;
}

yasm_linemap *
yasm_object_get_linemap(const yasm_object *object)
{
    return object->linemap;
}

int
yasm_section_is_absolute(yasm_section *sect)
{
    return (sect->type == SECTION_ABSOLUTE);
}

int
yasm_section_is_code(yasm_section *sect)
{
    return sect->code;
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

int
yasm_section_is_default(const yasm_section *sect)
{
    return sect->def;
}

void
yasm_section_set_default(yasm_section *sect, int def)
{
    sect->def = def;
}

yasm_object *
yasm_section_get_object(const yasm_section *sect)
{
    return sect->object;
}

void *
yasm_section_get_data(yasm_section *sect,
		      const yasm_assoc_data_callback *callback)
{
    return yasm__assoc_data_get(sect->assoc_data, callback);
}

void
yasm_section_add_data(yasm_section *sect,
		      const yasm_assoc_data_callback *callback, void *data)
{
    sect->assoc_data = yasm__assoc_data_add(sect->assoc_data, callback, data);
}

void
yasm_object_destroy(yasm_object *object)
{
    yasm_section *cur, *next;

    /* Delete sections */
    cur = STAILQ_FIRST(&object->sections);
    while (cur) {
	next = STAILQ_NEXT(cur, link);
	yasm_section_destroy(cur);
	cur = next;
    }

    /* Delete associated filenames */
    yasm_xfree(object->src_filename);
    yasm_xfree(object->obj_filename);

    /* Delete symbol table and line mappings */
    yasm_symtab_destroy(object->symtab);
    yasm_linemap_destroy(object->linemap);

    yasm_xfree(object);
}

void
yasm_object_print(const yasm_object *object, FILE *f, int indent_level)
{
    yasm_section *cur;

    /* Print symbol table */
    fprintf(f, "%*sSymbol Table:\n", indent_level, "");
    yasm_symtab_print(object->symtab, f, indent_level+1);

    /* Print sections and bytecodes */
    STAILQ_FOREACH(cur, &object->sections, link) {
	fprintf(f, "%*sSection:\n", indent_level, "");
	yasm_section_print(cur, f, indent_level+1, 1);
    }
}

void
yasm_object_finalize(yasm_object *object, yasm_errwarns *errwarns)
{
    yasm_section *sect;

    /* Iterate through sections */
    STAILQ_FOREACH(sect, &object->sections, link) {
	yasm_bytecode *cur = STAILQ_FIRST(&sect->bcs);
	yasm_bytecode *prev;

	/* Skip our locally created empty bytecode first. */
	prev = cur;
	cur = STAILQ_NEXT(cur, link);

	/* Iterate through the remainder, if any. */
	while (cur) {
	    /* Finalize */
	    yasm_bc_finalize(cur, prev);
	    yasm_errwarn_propagate(errwarns, cur->line);
	    prev = cur;
	    cur = STAILQ_NEXT(cur, link);
	}
    }
}

int
yasm_object_sections_traverse(yasm_object *object, /*@null@*/ void *d,
			      int (*func) (yasm_section *sect,
					   /*@null@*/ void *d))
{
    yasm_section *cur;

    STAILQ_FOREACH(cur, &object->sections, link) {
	int retval = func(cur, d);
	if (retval != 0)
	    return retval;
    }
    return 0;
}

/*@-onlytrans@*/
yasm_section *
yasm_object_find_general(yasm_object *object, const char *name)
{
    yasm_section *cur;

    STAILQ_FOREACH(cur, &object->sections, link) {
	if (cur->type == SECTION_GENERAL &&
	    strcmp(cur->data.general.name, name) == 0)
	    return cur;
    }
    return NULL;
}
/*@=onlytrans@*/

void
yasm_section_add_reloc(yasm_section *sect, yasm_reloc *reloc,
		       void (*destroy_func) (/*@only@*/ void *reloc))
{
    STAILQ_INSERT_TAIL(&sect->relocs, reloc, link);
    if (!destroy_func)
	yasm_internal_error(N_("NULL destroy function given to add_reloc"));
    else if (sect->destroy_reloc && destroy_func != sect->destroy_reloc)
	yasm_internal_error(N_("different destroy function given to add_reloc"));
    sect->destroy_reloc = destroy_func;
}

/*@null@*/ yasm_reloc *
yasm_section_relocs_first(yasm_section *sect)
{
    return STAILQ_FIRST(&sect->relocs);
}

#undef yasm_section_reloc_next
/*@null@*/ yasm_reloc *
yasm_section_reloc_next(yasm_reloc *reloc)
{
    return STAILQ_NEXT(reloc, link);
}

void
yasm_reloc_get(yasm_reloc *reloc, yasm_intnum **addrp, yasm_symrec **symp)
{
    *addrp = reloc->addr;
    *symp = reloc->sym;
}


yasm_bytecode *
yasm_section_bcs_first(yasm_section *sect)
{
    return STAILQ_FIRST(&sect->bcs);
}

yasm_bytecode *
yasm_section_bcs_last(yasm_section *sect)
{
    return STAILQ_LAST(&sect->bcs, yasm_bytecode, link);
}

yasm_bytecode *
yasm_section_bcs_append(yasm_section *sect, yasm_bytecode *bc)
{
    if (bc) {
	if (bc->callback) {
	    bc->section = sect;	    /* record parent section */
	    STAILQ_INSERT_TAIL(&sect->bcs, bc, link);
	    return bc;
	} else
	    yasm_xfree(bc);
    }
    return (yasm_bytecode *)NULL;
}

int
yasm_section_bcs_traverse(yasm_section *sect,
			  /*@null@*/ yasm_errwarns *errwarns,
			  /*@null@*/ void *d,
			  int (*func) (yasm_bytecode *bc, /*@null@*/ void *d))
{
    yasm_bytecode *cur = STAILQ_FIRST(&sect->bcs);

    /* Skip our locally created empty bytecode first. */
    cur = STAILQ_NEXT(cur, link);

    /* Iterate through the remainder, if any. */
    while (cur) {
	int retval = func(cur, d);
	if (errwarns)
	    yasm_errwarn_propagate(errwarns, cur->line);
	if (retval != 0)
	    return retval;
	cur = STAILQ_NEXT(cur, link);
    }
    return 0;
}

const char *
yasm_section_get_name(const yasm_section *sect)
{
    if (sect->type == SECTION_GENERAL)
	return sect->data.general.name;
    return NULL;
}

yasm_symrec *
yasm_section_abs_get_sym(const yasm_section *sect)
{
    if (sect->type == SECTION_ABSOLUTE)
	return sect->data.absolute.first;
    return NULL;
}

void
yasm_section_set_start(yasm_section *sect, yasm_expr *start,
		       unsigned long line)
{
    yasm_expr_destroy(sect->start);
    sect->start = start;
}

const yasm_expr *
yasm_section_get_start(const yasm_section *sect)
{
    return sect->start;
}

void
yasm_section_set_align(yasm_section *sect, unsigned long align,
		       unsigned long line)
{
    sect->align = align;
}

unsigned long
yasm_section_get_align(const yasm_section *sect)
{
    return sect->align;
}

static void
yasm_section_destroy(yasm_section *sect)
{
    yasm_bytecode *cur, *next;
    yasm_reloc *r_cur, *r_next;

    if (!sect)
	return;

    if (sect->type == SECTION_GENERAL) {
	yasm_xfree(sect->data.general.name);
    }
    yasm__assoc_data_destroy(sect->assoc_data);
    yasm_expr_destroy(sect->start);

    /* Delete bytecodes */
    cur = STAILQ_FIRST(&sect->bcs);
    while (cur) {
	next = STAILQ_NEXT(cur, link);
	yasm_bc_destroy(cur);
	cur = next;
    }

    /* Delete relocations */
    r_cur = STAILQ_FIRST(&sect->relocs);
    while (r_cur) {
	r_next = STAILQ_NEXT(r_cur, link);
	yasm_intnum_destroy(r_cur->addr);
	sect->destroy_reloc(r_cur);
	r_cur = r_next;
    }

    yasm_xfree(sect);
}

void
yasm_section_print(const yasm_section *sect, FILE *f, int indent_level,
		   int print_bcs)
{
    if (!sect) {
	fprintf(f, "%*s(none)\n", indent_level, "");
	return;
    }

    fprintf(f, "%*stype=", indent_level, "");
    switch (sect->type) {
	case SECTION_GENERAL:
	    fprintf(f, "general\n%*sname=%s\n", indent_level, "",
		    sect->data.general.name);
	    break;
	case SECTION_ABSOLUTE:
	    fprintf(f, "absolute\n");
	    break;
    }

    fprintf(f, "%*sstart=", indent_level, "");
    yasm_expr_print(sect->start, f);
    fprintf(f, "\n");

    if (sect->assoc_data) {
	fprintf(f, "%*sAssociated data:\n", indent_level, "");
	yasm__assoc_data_print(sect->assoc_data, f, indent_level+1);
    }

    if (print_bcs) {
	yasm_bytecode *cur;

	fprintf(f, "%*sBytecodes:\n", indent_level, "");

	STAILQ_FOREACH(cur, &sect->bcs, link) {
	    fprintf(f, "%*sNext Bytecode:\n", indent_level+1, "");
	    yasm_bc_print(cur, f, indent_level+2);
	}
    }
}

/*
 * Robertson (1977) optimizer
 * Based (somewhat loosely) on the algorithm given in:
 *   MRC Technical Summary Report # 1779
 *   CODE GENERATION FOR SHORT/LONG ADDRESS MACHINES
 *   Edward L. Robertson
 *   Mathematics Research Center
 *   University of Wisconsin-Madison
 *   610 Walnut Street
 *   Madison, Wisconsin 53706
 *   August 1977
 *
 * Key components of algorithm:
 *  - start assuming all short forms
 *  - build spans for short->long transition dependencies
 *  - if a long form is needed, walk the dependencies and update
 * Major differences from Robertson's algorithm:
 *  - detection of cycles
 *  - any difference of two locations is allowed
 *  - handling of alignment gaps
 *  - handling of multiples
 *
 * Data structures:
 *  - Interval tree to store spans and associated data
 *  - Queue Q
 *
 * Each span keeps track of:
 *  - Associated bytecode (bytecode that depends on the span length)
 *  - Active/inactive state (starts out active)
 *  - Sign (negative/positive; negative being "backwards" in address)
 *  - Current length in bytes
 *  - New length in bytes
 *  - Negative/Positive thresholds
 *  - Span ID (unique within each bytecode)
 *
 * How org and align are handled:
 * Some portions are critical values that must not depend on any bytecode
 * offset (either relative or absolute).
 *
 * ALIGN: 0 length (always).  Bump offset to alignment.  Span from 0 to
 *        align bytecode, update on any change.  If span length
 *        increases past alignment, increase offset by alignment and update
 *        dependent spans.  Alignment is critical value.
 * ORG:   Same as align, but if span's length exceeds org value, error.
 *        ORG value is critical value.
 *
 * How times is handled:
 *
 * TIMES: Handled separately from bytecode "raw" size.  If not span-dependent,
 *        trivial (just multiplied in at any bytecode size increase).  Span
 *        dependent times update on any change (span ID 0).  If the resultant
 *        next bytecode offset would be less than the old next bytecode offset,
 *        error.  Otherwise increase offset and update dependent spans.
 *
 * To reduce interval tree size, a first expansion pass is performed
 * before the spans are added to the tree.
 *
 * Basic algorithm outline:
 *
 * 1. Initialization:
 *  a. Number bytecodes sequentially (via bc_index) and calculate offsets
 *     of all bytecodes assuming minimum length, building a list of all
 *     dependent spans as we go.
 *     "minimum" here means absolute minimum:
 *      - align 0 length
 *      - times values (with span-dependent values) assumed to be 0
 *      - org bumps offset
 *  b. Iterate over spans.  Set span length based on bytecode offsets
 *     determined in 1a.  If span is "certainly" long because the span
 *     is an absolute reference to another section (or external) or the
 *     distance calculated based on the minimum length is greater than the
 *     span's threshold, expand the span's bytecode, and if no further
 *     expansion can result, delete the span.  Otherwise (or if the
 *     expansion results in another threshold of expansion), add span to
 *     interval tree.
 *  c. Iterate over bytecodes to update all bytecode offsets based on new
 *     (expanded) lengths calculated in 1b.
 *  d. Iterate over spans.  Update span's length based on new bytecode offsets
 *     determined in 1c.  If span's length exceeds long threshold, add that
 *     span to Q.
 * 2. Main loop:
 *   While Q not empty:
 *     Expand BC dependent on span at head of Q (and remove span from Q).
 *     Update span:
 *       If BC no longer dependent on span, mark span as inactive.
 *       If BC has new thresholds for span, update span.
 *     If BC increased in size, for each active span that contains BC:
 *       Increase span length by difference between short and long BC length.
 *       If span exceeds long threshold (or is flagged to recalculate on any
 *       change), add it to tail of Q.
 * 3. Final pass over bytecodes to generate final offsets.
 */

typedef struct yasm_span {
    /*@reldef@*/ STAILQ_ENTRY(yasm_span) link;

    /*@dependent@*/ yasm_bytecode *bc;

    yasm_value *depval;
    yasm_bytecode *origin_prevbc;

    /* Special handling: see descriptions above */
    enum {
	NOT_SPECIAL = 0,
	SPECIAL_ALIGN,
	SPECIAL_ORG,
	SPECIAL_TIMES
    } special;

    long cur_val;
    long new_val;

    long neg_thres;
    long pos_thres;

    int id;

    int active;
} yasm_span;

typedef struct optimize_data {
    /*@reldef@*/ STAILQ_HEAD(yasm_spanhead, yasm_span) spans;
} optimize_data;

static void
optimize_add_span(void *add_span_data, yasm_bytecode *bc, int id,
		  yasm_value *value, /*@null@*/ yasm_bytecode *origin_prevbc,
		  long neg_thres, long pos_thres)
{
    optimize_data *optd = (optimize_data *)add_span_data;
    yasm_span *span = yasm_xmalloc(sizeof(yasm_span));

    span->bc = bc;
    span->depval = value;
    span->origin_prevbc = origin_prevbc;
    span->special = NOT_SPECIAL;
    span->cur_val = 0;
    span->new_val = 0;
    span->neg_thres = neg_thres;
    span->pos_thres = pos_thres;
    span->id = id;
    span->active = 1;

    STAILQ_INSERT_TAIL(&optd->spans, span, link);
}

static void
update_all_bc_offsets(yasm_object *object)
{
    yasm_section *sect;

    STAILQ_FOREACH(sect, &object->sections, link) {
	unsigned long offset = 0;

	yasm_bytecode *cur = STAILQ_FIRST(&sect->bcs);
	yasm_bytecode *prev;

	/* Skip our locally created empty bytecode first. */
	prev = cur;
	cur = STAILQ_NEXT(cur, link);

	/* Iterate through the remainder, if any. */
	while (cur) {
	    cur->offset = offset;
	    offset += cur->len;
	    prev = cur;
	    cur = STAILQ_NEXT(cur, link);
	}
    }
}

void
yasm_object_optimize(yasm_object *object, yasm_arch *arch)
{
    yasm_section *sect;
    unsigned long bc_index = 0;
    int saw_error = 0;
    optimize_data optd;
    yasm_span *span;
    long neg_thres, pos_thres;

    STAILQ_INIT(&optd.spans);

    /* Step 1a */
    STAILQ_FOREACH(sect, &object->sections, link) {
	unsigned long offset = 0;

	yasm_bytecode *cur = STAILQ_FIRST(&sect->bcs);
	yasm_bytecode *prev;

	cur->bc_index = bc_index++;

	/* Skip our locally created empty bytecode first. */
	prev = cur;
	cur = STAILQ_NEXT(cur, link);

	/* Iterate through the remainder, if any. */
	while (cur) {
	    cur->bc_index = bc_index++;

	    if (yasm_bc_calc_len(cur, optimize_add_span, &optd))
		saw_error = 1;

	    /* TODO: times */
	    if (cur->multiple)
		yasm_internal_error("multiple not yet supported");

	    cur->offset = offset;
	    offset += cur->len;
	    prev = cur;
	    cur = STAILQ_NEXT(cur, link);
	}
    }

    if (saw_error)
	return;
#if 0
    /* Step 1b */
    STAILQ_FOREACH(span, &optd.spans, link) {
	/* Handle absolute portion */
	if (span->depval->abs) {
	    yasm_expr *depcopy = yasm_expr_copy(span->depval->abs);
	    yasm_intnum *intn =
		yasm_expr_get_intnum(&depcopy, yasm_common_calc_bc_dist);
	    if (intn)
		span->new_val = yasm_intnum_get_int(intn);
	    else {
		/* absolute, external, or too complex; force to longer form */
		span->new_val = LONG_MAX;
		span->active = 0;
	    }
	    yasm_expr_destroy(depcopy);
	} else
	    span->new_val = 0;

	/* Handle relative portion */
	if (span->depval->rel && span->new_val != LONG_MAX) {
	    span->new_val += 
	}

	if (span->new_val < span->neg_thres
	    || span->new_val > span->pos_thres) {
	    int retval = yasm_bc_expand(span->bc, span->id, span->cur_val,
					span->new_val, &neg_thres, &pos_thres);
	    if (retval < 0)
		saw_error = 1;
	    else if (retval > 0) {
		span->neg_thres = neg_thres;
		span->pos_thres = pos_thres;
	    } else
		span->active = 0;
	}
	span->cur_val = span->new_val;
    }
#endif
    if (saw_error)
	return;

    /* Step 1c */
    update_all_bc_offsets(object);

    /* Step 1d */
    STAILQ_FOREACH(span, &optd.spans, link) {
	if (!span->active)
	    continue;
    }

    /* Step 2 */

    /* Step 3 */
    update_all_bc_offsets(object);
}
