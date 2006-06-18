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

#include "linemgr.h"
#include "errwarn.h"
#include "intnum.h"
#include "expr.h"
#include "value.h"
#include "symrec.h"

#include "bytecode.h"
#include "arch.h"
#include "section.h"
#include "objfmt.h"

#include "expr-int.h"
#include "bc-int.h"

#include "inttree.h"


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
    bc->offset = 0;
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
    bc->offset = 0;
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
 * ALIGN/ORG/other offset-based bytecodes: Length set to bump offset to
 *	alignment.  Span from 0 to bytecode, update on any change.  Can
 *	update to new offset for things like align, which gets propagated
 *	to update dependent spans.  Alignment/ORG value is critical value.
 *	Cannot be combined with TIMES.
 *
 * How times is handled:
 *
 * TIMES: Handled separately from bytecode "raw" size.  If not span-dependent,
 *	trivial (just multiplied in at any bytecode size increase).  Span
 *	dependent times update on any change (span ID 0).  If the resultant
 *	next bytecode offset would be less than the old next bytecode offset,
 *	error.  Otherwise increase offset and update dependent spans.
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
 *      - align/org (offset-based) bumps offset as normal
 *      - times values (with span-dependent values) assumed to be 0
 *  b. Iterate over spans.  Set span length based on bytecode offsets
 *     determined in 1a.  If span is "certainly" long because the span
 *     is an absolute reference to another section (or external) or the
 *     distance calculated based on the minimum length is greater than the
 *     span's threshold, expand the span's bytecode, and if no further
 *     expansion can result, mark span as inactive.
 *  c. Iterate over bytecodes to update all bytecode offsets based on new
 *     (expanded) lengths calculated in 1b.
 *  d. Iterate over active spans.  Add span to interval tree.  Update span's
 *     length based on new bytecode offsets determined in 1c.  If span's
 *     length exceeds long threshold, add that span to Q.
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

typedef struct yasm_span yasm_span;

typedef struct yasm_span_term {
    yasm_bytecode *precbc, *precbc2;
    yasm_span *span;	    /* span this term is a member of */
    long cur_val, new_val;
    unsigned int subst;
} yasm_span_term;

struct yasm_span {
    /*@reldef@*/ TAILQ_ENTRY(yasm_span) link;	/* for allocation tracking */
    /*@reldef@*/ STAILQ_ENTRY(yasm_span) linkq;	/* for Q */

    /*@dependent@*/ yasm_bytecode *bc;

    yasm_value depval;

    /* span term for relative portion of value */
    yasm_span_term *rel_term;
    /* span terms in absolute portion of value */
    yasm_span_term *terms;
    yasm_expr__item *items;
    unsigned int num_terms;

    /* Special handling: see descriptions above */
    enum {
	NOT_SPECIAL = 0,
	SPECIAL_BC_OFFSET,
	SPECIAL_TIMES
    } special;

    long cur_val;
    long new_val;

    long neg_thres;
    long pos_thres;

    int id;

    int active;
};

typedef struct optimize_data {
    /*@reldef@*/ TAILQ_HEAD(, yasm_span) spans;
    /*@reldef@*/ STAILQ_HEAD(, yasm_span) QA, QB;
    /*@only@*/ IntervalTree *itree;
    long len_diff;	/* used only for optimize_term_expand */
} optimize_data;

static void
optimize_add_span(void *add_span_data, yasm_bytecode *bc, int id,
		  const yasm_value *value, long neg_thres, long pos_thres)
{
    optimize_data *optd = (optimize_data *)add_span_data;
    yasm_span *span = yasm_xmalloc(sizeof(yasm_span));

    span->bc = bc;
    yasm_value_init_copy(&span->depval, value);
    span->rel_term = NULL;
    span->terms = NULL;
    span->items = NULL;
    span->num_terms = 0;
    span->special = NOT_SPECIAL;
    span->cur_val = 0;
    span->new_val = 0;
    span->neg_thres = neg_thres;
    span->pos_thres = pos_thres;
    span->id = id;
    span->active = 1;

    TAILQ_INSERT_TAIL(&optd->spans, span, link);
}

static void
add_span_term(unsigned int subst, yasm_bytecode *precbc,
	      yasm_bytecode *precbc2, void *d)
{
    yasm_span *span = d;
    yasm_intnum *intn;

    if (subst >= span->num_terms) {
	/* Linear expansion since total number is essentially always small */
	span->num_terms = subst+1;
	span->terms = yasm_xrealloc(span->terms,
				    span->num_terms*sizeof(yasm_span_term));
    }
    span->terms[subst].precbc = precbc;
    span->terms[subst].precbc2 = precbc2;
    span->terms[subst].span = span;
    span->terms[subst].subst = subst;

    intn = yasm_calc_bc_dist(precbc, precbc2);
    if (!intn)
	yasm_internal_error(N_("could not calculate bc distance"));
    span->terms[subst].cur_val = 0;
    span->terms[subst].new_val = yasm_intnum_get_int(intn);
    yasm_intnum_destroy(intn);
}

static void
span_create_terms(yasm_span *span)
{
    unsigned int i;
    yasm_intnum *intn;

    /* Split out sym-sym terms in absolute portion of dependent value */
    if (span->depval.abs) {
	span->num_terms = yasm_expr__bc_dist_subst(&span->depval.abs, span,
						   add_span_term);
	if (span->num_terms > 0) {
	    span->items = yasm_xmalloc(span->num_terms*sizeof(yasm_expr__item));
	    for (i=0; i<span->num_terms; i++) {
		/* Create items with dummy value */
		span->items[i].type = YASM_EXPR_INT;
		span->items[i].data.intn = yasm_intnum_create_int(0);
	    }
	}
    }

    /* Create term for relative portion of dependent value */
    if (span->depval.rel) {
	yasm_bytecode *rel_precbc;
	int sym_local;

	sym_local = yasm_symrec_get_label(span->depval.rel, &rel_precbc);
	if (span->depval.wrt || span->depval.seg_of || span->depval.section_rel
	    || !sym_local)
	    return;	/* we can't handle SEG, WRT, or external symbols */
	if (rel_precbc->section != span->bc->section)
	    return;	/* not in this section */
	if (!span->depval.curpos_rel)
	    return;	/* not PC-relative */

	span->rel_term = yasm_xmalloc(sizeof(yasm_span_term));
	span->rel_term->precbc = rel_precbc;
	span->rel_term->precbc2 = NULL;
	span->rel_term->span = span;
	span->rel_term->subst = ~0U;

	span->rel_term->cur_val = 0;
	span->rel_term->new_val =
	    rel_precbc->offset + rel_precbc->len - span->bc->offset;
    }
}

/* Recalculate span value based on current span replacement values.
 * Returns 1 if span exceeded thresholds.
 */
static int
recalc_normal_span(yasm_span *span)
{
    span->new_val = 0;

    if (span->depval.abs) {
	yasm_expr *abs_copy = yasm_expr_copy(span->depval.abs);
	/*@null@*/ /*@dependent@*/ yasm_intnum *num;

	/* Update sym-sym terms and substitute back into expr */
	unsigned int i;
	for (i=0; i<span->num_terms; i++)
	    yasm_intnum_set_int(span->items[i].data.intn,
				span->terms[i].new_val);
	yasm_expr__subst(abs_copy, span->num_terms, span->items);
	num = yasm_expr_get_intnum(&abs_copy, 0);
	if (num)
	    span->new_val = yasm_intnum_get_int(num);
	else
	    span->new_val = LONG_MAX; /* too complex; force to longest form */
	yasm_expr_destroy(abs_copy);
    }

    if (span->rel_term && span->new_val != LONG_MAX
	&& span->rel_term->new_val != LONG_MAX)
	span->new_val += span->rel_term->new_val >> span->depval.rshift;
    else
	span->new_val = LONG_MAX;   /* too complex; force to longest form */

    if (span->new_val == LONG_MAX)
	span->active = 0;

    return (span->new_val < span->neg_thres
	    || span->new_val > span->pos_thres);
}

/* Updates all bytecode offsets.  For offset-based bytecodes, calls expand
 * to determine new length.
 */
static int
update_all_bc_offsets(yasm_object *object, yasm_errwarns *errwarns)
{
    yasm_section *sect;
    int saw_error = 0;

    STAILQ_FOREACH(sect, &object->sections, link) {
	unsigned long offset = 0;

	yasm_bytecode *bc = STAILQ_FIRST(&sect->bcs);
	yasm_bytecode *prevbc;

	/* Skip our locally created empty bytecode first. */
	prevbc = bc;
	bc = STAILQ_NEXT(bc, link);

	/* Iterate through the remainder, if any. */
	while (bc) {
	    if (bc->callback->special == YASM_BC_SPECIAL_OFFSET) {
		/* Recalculate/adjust len of offset-based bytecodes here */
		long neg_thres = 0;
		long pos_thres = bc->offset+bc->len;
		int retval = yasm_bc_expand(bc, 0, 0,
					    (long)(prevbc->offset+prevbc->len),
					    &neg_thres, &pos_thres);
		yasm_errwarn_propagate(errwarns, bc->line);
		if (retval < 0)
		    saw_error = 1;
	    }
	    bc->offset = offset;
	    offset += bc->len;
	    prevbc = bc;
	    bc = STAILQ_NEXT(bc, link);
	}
    }
    return saw_error;
}

static void
span_destroy(/*@only@*/ yasm_span *span)
{
    unsigned int i;

    yasm_value_delete(&span->depval);
    if (span->rel_term)
	yasm_xfree(span->rel_term);
    if (span->terms)
	yasm_xfree(span->terms);
    if (span->items) {
	for (i=0; i<span->num_terms; i++)
	    yasm_intnum_destroy(span->items[i].data.intn);
	yasm_xfree(span->items);
    }
    yasm_xfree(span);
}

static void
optimize_cleanup(optimize_data *optd)
{
    yasm_span *s1, *s2;

    IT_destroy(optd->itree);

    s1 = TAILQ_FIRST(&optd->spans);
    while (s1) {
	s2 = TAILQ_NEXT(s1, link);
	span_destroy(s1);
	s1 = s2;
    }
}

static void
optimize_term_expand(IntervalTreeNode *node, void *d)
{
    optimize_data *optd = d;
    yasm_span_term *term = node->data;
    yasm_span *span = term->span;
    long len_diff = optd->len_diff;
    long precbc_index, precbc2_index;

    /* Don't expand inactive spans */
    if (!span->active)
	return;

    /* Update term length */
    if (term->precbc)
	precbc_index = term->precbc->bc_index;
    else
	precbc_index = span->bc->bc_index-1;

    if (term->precbc2)
	precbc2_index = term->precbc2->bc_index;
    else
	precbc2_index = span->bc->bc_index-1;

    if (precbc_index > precbc2_index)
	term->new_val += len_diff;
    else
	term->new_val -= len_diff;

    /* If already on Q, don't re-add */
    if (span->active == 2)
	return;

    /* Update term and check against thresholds */
    if (!recalc_normal_span(span))
	return;	/* didn't exceed thresholds, we're done */

    /* Exceeded thresholds, probably need to add to Q for expansion */
    switch (span->special) {
	case NOT_SPECIAL:
	    STAILQ_INSERT_TAIL(&optd->QB, span, linkq);
	    span->active = 2;	    /* Mark as being in Q */
	    break;
	case SPECIAL_BC_OFFSET:
	    STAILQ_INSERT_TAIL(&optd->QA, span, linkq);
	    span->active = 2;	    /* Mark as being in Q */
	    break;
	case SPECIAL_TIMES:
	    break;
    }
}

void
yasm_object_optimize(yasm_object *object, yasm_arch *arch,
		     yasm_errwarns *errwarns)
{
    yasm_section *sect;
    unsigned long bc_index = 0;
    int saw_error = 0;
    optimize_data optd;
    yasm_span *span, *span_temp;
    int retval;
    unsigned int i;

    TAILQ_INIT(&optd.spans);
    optd.itree = IT_create();

    /* Step 1a */
    STAILQ_FOREACH(sect, &object->sections, link) {
	unsigned long offset = 0;

	yasm_bytecode *bc = STAILQ_FIRST(&sect->bcs);
	yasm_bytecode *prevbc;

	bc->bc_index = bc_index++;

	/* Skip our locally created empty bytecode first. */
	prevbc = bc;
	bc = STAILQ_NEXT(bc, link);

	/* Iterate through the remainder, if any. */
	while (bc) {
	    bc->bc_index = bc_index++;
	    bc->offset = offset;

	    retval = yasm_bc_calc_len(bc, optimize_add_span, &optd);
	    yasm_errwarn_propagate(errwarns, bc->line);
	    if (retval)
		saw_error = 1;
	    else {
		if (bc->callback->special == YASM_BC_SPECIAL_OFFSET) {
		    span = yasm_xmalloc(sizeof(yasm_span));

		    span->bc = bc;
		    yasm_value_initialize(&span->depval, NULL, 0);
		    span->rel_term = NULL;
		    span->terms = NULL;
		    span->items = NULL;
		    span->num_terms = 0;
		    span->special = SPECIAL_BC_OFFSET;
		    span->cur_val = (long)bc->offset;
		    span->new_val = 0;
		    span->neg_thres = 0;
		    span->pos_thres = (long)(bc->offset+bc->len);
		    span->id = 0;
		    span->active = 1;

		    TAILQ_INSERT_TAIL(&optd.spans, span, link);
		    if (bc->multiple) {
			yasm_error_set(YASM_ERROR_VALUE,
			    N_("cannot combine multiples and setting assembly position"));
			yasm_errwarn_propagate(errwarns, span->bc->line);
			saw_error = 1;
		    }
		}
		/* TODO: times */
#if 0
		if (bc->len != 0 && bc->multiple) {
		    yasm_internal_error("multiple not yet supported");
		}
#endif
		offset += bc->len;
	    }

	    prevbc = bc;
	    bc = STAILQ_NEXT(bc, link);
	}
    }

    if (saw_error) {
	optimize_cleanup(&optd);
	return;
    }

    /* Step 1b */
    TAILQ_FOREACH_SAFE(span, &optd.spans, link, span_temp) {
	switch (span->special) {
	    case NOT_SPECIAL:
		span_create_terms(span);
		if (recalc_normal_span(span)) {
		    retval = yasm_bc_expand(span->bc, span->id, span->cur_val,
					    span->new_val, &span->neg_thres,
					    &span->pos_thres);
		    yasm_errwarn_propagate(errwarns, span->bc->line);
		    if (retval < 0)
			saw_error = 1;
		    else if (retval > 0) {
			if (!span->active) {
			    yasm_error_set(YASM_ERROR_VALUE,
				N_("secondary expansion of an external/complex value"));
			    yasm_errwarn_propagate(errwarns, span->bc->line);
			    saw_error = 1;
			}
		    } else {
			TAILQ_REMOVE(&optd.spans, span, link);
			span_destroy(span);
			continue;
		    }
		}
		span->cur_val = span->new_val;
		break;
	    case SPECIAL_BC_OFFSET:
		/* Create term */
		span->rel_term = yasm_xmalloc(sizeof(yasm_span_term));
		span->rel_term->precbc = NULL;
		span->rel_term->precbc2 = STAILQ_FIRST(&span->bc->section->bcs);
		span->rel_term->span = span;
		span->rel_term->subst = ~0U;
		span->rel_term->cur_val = 0;
		span->rel_term->new_val = (long)span->bc->offset;
		break;
	    case SPECIAL_TIMES:
		break;
	}
    }

    if (saw_error) {
	optimize_cleanup(&optd);
	return;
    }

    /* Step 1c */
    if (update_all_bc_offsets(object, errwarns)) {
	optimize_cleanup(&optd);
	return;
    }

    /* Step 1d */
    STAILQ_INIT(&optd.QB);
    TAILQ_FOREACH(span, &optd.spans, link) {
	yasm_intnum *intn;

	/* Update span terms based on new bc offsets */
	for (i=0; i<span->num_terms; i++) {
	    intn = yasm_calc_bc_dist(span->terms[i].precbc,
				     span->terms[i].precbc2);
	    if (!intn)
		yasm_internal_error(N_("could not calculate bc distance"));
	    span->terms[i].cur_val = span->terms[i].new_val;
	    span->terms[i].new_val = yasm_intnum_get_int(intn);
	    yasm_intnum_destroy(intn);
	}
	if (span->rel_term) {
	    span->rel_term->cur_val = span->rel_term->new_val;
	    if (span->rel_term->precbc)
		span->rel_term->new_val = span->rel_term->precbc->offset
		    + span->rel_term->precbc->len - span->bc->offset;
	    else
		span->rel_term->new_val = span->bc->offset -
		    (span->rel_term->precbc2->offset +
		     span->rel_term->precbc2->len);
	}

	switch (span->special) {
	    case NOT_SPECIAL:
		if (recalc_normal_span(span)) {
		    /* Exceeded threshold, add span to QB */
		    STAILQ_INSERT_TAIL(&optd.QB, span, linkq);
		}
		break;
	    case SPECIAL_BC_OFFSET:
		/* It's impossible to exceed any threshold here (as we just
		 * adjusted this when updating the BC offsets), so just update
		 * span values and thresholds.
		 */
		span->new_val = span->rel_term->new_val;
		span->pos_thres = (long)(span->bc->offset+span->bc->len);

		span->rel_term->cur_val = span->rel_term->new_val;
		span->cur_val = span->new_val;
		break;
	    case SPECIAL_TIMES:
		break;
	}
    }

    /* Do we need step 2?  If not, go ahead and exit. */
    if (STAILQ_EMPTY(&optd.QB)) {
	optimize_cleanup(&optd);
	return;
    }

    /* Build up interval tree */
    TAILQ_FOREACH(span, &optd.spans, link) {
	for (i=0; i<span->num_terms; i++)
	    IT_insert(optd.itree, (long)span->terms[i].precbc->bc_index,
		      (long)span->terms[i].precbc2->bc_index, &span->terms[i]);
	if (span->rel_term) {
	    if (span->rel_term->precbc)
		IT_insert(optd.itree, (long)span->rel_term->precbc->bc_index,
			  (long)span->bc->bc_index-1, span->rel_term);
	    else
		IT_insert(optd.itree, (long)span->rel_term->precbc2->bc_index,
			  (long)span->bc->bc_index-1, span->rel_term);
	}
    }

    /* Step 2 */
    STAILQ_INIT(&optd.QA);
    while (!STAILQ_EMPTY(&optd.QA) || !(STAILQ_EMPTY(&optd.QB))) {
	unsigned long orig_len;

	/* QA is for offset BCs, update those first, then update non-offset.
	 * This is so that offset BCs can absorb increases before we look at
	 * expanding non-offset BCs.
	 */
	if (!STAILQ_EMPTY(&optd.QA)) {
	    span = STAILQ_FIRST(&optd.QA);
	    STAILQ_REMOVE_HEAD(&optd.QA, linkq);
	} else {
	    span = STAILQ_FIRST(&optd.QB);
	    STAILQ_REMOVE_HEAD(&optd.QB, linkq);
	}

	if (!span->active)
	    continue;
	span->active = 1;   /* no longer in Q */

	/* Make sure we ended up ultimately exceeding thresholds; due to
	 * offset BCs we may have been placed on Q and then reduced in size
	 * again.
	 */
	if (!recalc_normal_span(span))
	    continue;

	orig_len = span->bc->len;

	retval = yasm_bc_expand(span->bc, span->id, span->cur_val,
				span->new_val, &span->neg_thres,
				&span->pos_thres);
	yasm_errwarn_propagate(errwarns, span->bc->line);

	if (retval < 0) {
	    /* error */
	    saw_error = 1;
	    continue;
	} else if (retval > 0) {
	    /* another threshold, keep active */
	    for (i=0; i<span->num_terms; i++)
		span->terms[i].cur_val = span->terms[i].new_val;
	    if (span->rel_term)
		span->rel_term->cur_val = span->rel_term->new_val;
	    span->cur_val = span->new_val;
	} else
	    span->active = 0;	    /* we're done with this span */

	optd.len_diff = span->bc->len - orig_len;
	if (optd.len_diff == 0)
	    continue;	/* didn't increase in size */

	/* Iterate over all spans dependent across the bc just expanded */
	IT_enumerate(optd.itree, (long)span->bc->bc_index,
		     (long)span->bc->bc_index, &optd, optimize_term_expand);
    }

    if (saw_error) {
	optimize_cleanup(&optd);
	return;
    }

    /* Step 3 */
    update_all_bc_offsets(object, errwarns);
    optimize_cleanup(&optd);
}
