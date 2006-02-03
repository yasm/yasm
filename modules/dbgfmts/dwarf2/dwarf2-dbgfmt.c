/*
 * DWARF2 debugging format
 *
 *  Copyright (C) 2006  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of other contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
/*@unused@*/ RCSID("$Id$");

#define YASM_LIB_INTERNAL
#define YASM_BC_INTERNAL
#include <libyasm.h>

#define WITH_DWARF3 1

/* DWARF line number opcodes */
typedef enum {
    DW_LNS_extended_op = 0,
    DW_LNS_copy,
    DW_LNS_advance_pc,
    DW_LNS_advance_line,
    DW_LNS_set_file,
    DW_LNS_set_column,
    DW_LNS_negate_stmt,
    DW_LNS_set_basic_block,
    DW_LNS_const_add_pc,
    DW_LNS_fixed_advance_pc,
#ifdef WITH_DWARF3
    /* DWARF 3 extensions */
    DW_LNS_set_prologue_end,
    DW_LNS_set_epilogue_begin,
    DW_LNS_set_isa,
#endif
    DWARF2_LINE_OPCODE_BASE
} dwarf_line_number_op;

/* # of LEB128 operands needed for each of the above opcodes */
static unsigned char line_opcode_num_operands[DWARF2_LINE_OPCODE_BASE-1] = {
    0,	/* DW_LNS_copy */
    1,	/* DW_LNS_advance_pc */
    1,	/* DW_LNS_advance_line */
    1,	/* DW_LNS_set_file */
    1,	/* DW_LNS_set_column */
    0,	/* DW_LNS_negate_stmt */
    0,	/* DW_LNS_set_basic_block */
    0,	/* DW_LNS_const_add_pc */
    1,	/* DW_LNS_fixed_advance_pc */
#ifdef WITH_DWARF3
    0,	/* DW_LNS_set_prologue_end */
    0,	/* DW_LNS_set_epilogue_begin */
    1	/* DW_LNS_set_isa */
#endif
};

/* Line number extended opcodes */
typedef enum {
    DW_LNE_end_sequence = 1,
    DW_LNE_set_address,
    DW_LNE_define_file
} dwarf_line_number_ext_op;

/* Base and range for line offsets in special opcodes */
#define DWARF2_LINE_BASE		-5
#define DWARF2_LINE_RANGE		14

#define DWARF2_MAX_SPECIAL_ADDR_DELTA	\
    (((255-DWARF2_LINE_OPCODE_BASE)/DWARF2_LINE_RANGE)*\
     dbgfmt_dwarf2->min_insn_len)

/* Initial value of is_stmt register */
#define DWARF2_LINE_DEFAULT_IS_STMT	1

typedef struct {
    char *filename;	    /* basename of full filename */
    size_t dir;		    /* index into directories array for relative path;
			     * 0 for current directory. */
} dwarf2_filename;

/* Global data */
typedef struct yasm_dbgfmt_dwarf2 {
    yasm_dbgfmt_base dbgfmt;	    /* base structure */

    yasm_object *object;
    yasm_symtab *symtab;
    yasm_linemap *linemap;
    yasm_arch *arch;

    char **dirs;
    size_t dirs_size;
    size_t dirs_allocated;

    dwarf2_filename *filenames;
    size_t filenames_size;
    size_t filenames_allocated;

    enum {
	DWARF2_FORMAT_32BIT,
	DWARF2_FORMAT_64BIT
    } format;

    size_t sizeof_address, sizeof_offset, min_insn_len;
} yasm_dbgfmt_dwarf2;

/* .loc directive data */
typedef struct dwarf2_loc {
    /*@reldef@*/ STAILQ_ENTRY(dwarf2_loc) link;

    unsigned long vline;    /* virtual line number of .loc directive */

    /* source information */
    unsigned long file;	    /* index into table of filenames */
    unsigned long line;	    /* source line number */
    unsigned long column;   /* source column */
    int isa_change;
    unsigned long isa;
    enum {
	IS_STMT_NOCHANGE = 0,
	IS_STMT_SET,
	IS_STMT_CLEAR
    } is_stmt;
    int basic_block;
    int prologue_end;
    int epilogue_begin;

    yasm_bytecode *bc;	    /* first bytecode following */
    yasm_symrec *sym;	    /* last symbol preceding */
} dwarf2_loc;

/* Line number state machine register state */
typedef struct dwarf2_line_state {
    /* static configuration */
    yasm_dbgfmt_dwarf2 *dbgfmt_dwarf2;

    /* DWARF2 state machine registers */
    unsigned long address;
    unsigned long file;
    unsigned long line;
    unsigned long column;
    unsigned long isa;
    int is_stmt;

    /* other state information */
    /*@null@*/ yasm_bytecode *precbc;
} dwarf2_line_state;

/* Per-section data */
typedef struct dwarf2_section_data {
    /* The locations set by the .loc directives in this section, in assembly
     * source order.
     */
    /*@reldef@*/ STAILQ_HEAD(dwarf2_lochead, dwarf2_loc) locs;
} dwarf2_section_data;

/* Temporary information used during generate phase */
typedef struct dwarf2_info {
    yasm_section *debug_line;	/* section to which line number info goes */
    yasm_dbgfmt_dwarf2 *dbgfmt_dwarf2;
} dwarf2_info;

typedef struct dwarf2_spp {
    yasm_dbgfmt_dwarf2 *dbgfmt_dwarf2;
    yasm_bytecode *line_start_prevbc;
    yasm_bytecode *line_end_prevbc;
} dwarf2_spp;

typedef struct dwarf2_line_op {
    dwarf_line_number_op opcode;
    /*@owned@*/ /*@null@*/ yasm_intnum *operand;

    /* extended opcode */
    dwarf_line_number_ext_op ext_opcode;
    /*@owned@*/ /*@null@*/ yasm_expr *ext_operand;  /* unsigned */
    unsigned long ext_operandsize;
} dwarf2_line_op;

/* Section data callback function prototypes */
static void dwarf2_section_data_destroy(void *data);
static void dwarf2_section_data_print(void *data, FILE *f, int indent_level);

/* Bytecode callback function prototypes */

static void dwarf2_line_op_bc_destroy(void *contents);
static void dwarf2_line_op_bc_print(const void *contents, FILE *f, int
				    indent_level);
static yasm_bc_resolve_flags dwarf2_line_op_bc_resolve
    (yasm_bytecode *bc, int save, yasm_calc_bc_dist_func calc_bc_dist);
static int dwarf2_line_op_bc_tobytes
    (yasm_bytecode *bc, unsigned char **bufp, void *d,
     yasm_output_expr_func output_expr,
     /*@null@*/ yasm_output_reloc_func output_reloc);

static void dwarf2_spp_bc_destroy(void *contents);
static void dwarf2_spp_bc_print(const void *contents, FILE *f, int
				indent_level);
static yasm_bc_resolve_flags dwarf2_spp_bc_resolve
    (yasm_bytecode *bc, int save, yasm_calc_bc_dist_func calc_bc_dist);
static int dwarf2_spp_bc_tobytes
    (yasm_bytecode *bc, unsigned char **bufp, void *d,
     yasm_output_expr_func output_expr,
     /*@null@*/ yasm_output_reloc_func output_reloc);

/* Section data callback */
static const yasm_assoc_data_callback dwarf2_section_data_cb = {
    dwarf2_section_data_destroy,
    dwarf2_section_data_print
};

/* Bytecode callback structures */

static const yasm_bytecode_callback dwarf2_line_op_bc_callback = {
    dwarf2_line_op_bc_destroy,
    dwarf2_line_op_bc_print,
    yasm_bc_finalize_common,
    dwarf2_line_op_bc_resolve,
    dwarf2_line_op_bc_tobytes
};

static const yasm_bytecode_callback dwarf2_spp_bc_callback = {
    dwarf2_spp_bc_destroy,
    dwarf2_spp_bc_print,
    yasm_bc_finalize_common,
    dwarf2_spp_bc_resolve,
    dwarf2_spp_bc_tobytes
};

yasm_dbgfmt_module yasm_dwarf2_LTX_dbgfmt;


static /*@null@*/ /*@only@*/ yasm_dbgfmt *
dwarf2_dbgfmt_create(yasm_object *object, yasm_objfmt *of, yasm_arch *a)
{
    yasm_dbgfmt_dwarf2 *dbgfmt_dwarf2 =
	yasm_xmalloc(sizeof(yasm_dbgfmt_dwarf2));
    size_t i;

    dbgfmt_dwarf2->dbgfmt.module = &yasm_dwarf2_LTX_dbgfmt;

    dbgfmt_dwarf2->object = object;
    dbgfmt_dwarf2->symtab = yasm_object_get_symtab(object);
    dbgfmt_dwarf2->linemap = yasm_object_get_linemap(object);
    dbgfmt_dwarf2->arch = a;

    dbgfmt_dwarf2->dirs_allocated = 32;
    dbgfmt_dwarf2->dirs_size = 0;
    dbgfmt_dwarf2->dirs =
	yasm_xmalloc(sizeof(char *)*dbgfmt_dwarf2->dirs_allocated);

    dbgfmt_dwarf2->filenames_allocated = 32;
    dbgfmt_dwarf2->filenames_size = 0;
    dbgfmt_dwarf2->filenames =
	yasm_xmalloc(sizeof(dwarf2_filename)*dbgfmt_dwarf2->filenames_allocated);
    for (i=0; i<dbgfmt_dwarf2->filenames_allocated; i++) {
	dbgfmt_dwarf2->filenames[i].filename = NULL;
	dbgfmt_dwarf2->filenames[i].dir = 0;
    }

    dbgfmt_dwarf2->format = DWARF2_FORMAT_32BIT;    /* TODO: flexible? */

    dbgfmt_dwarf2->sizeof_address = yasm_arch_get_address_size(a)/8;
    switch (dbgfmt_dwarf2->format) {
	case DWARF2_FORMAT_32BIT:
	    dbgfmt_dwarf2->sizeof_offset = 4;
	    break;
	case DWARF2_FORMAT_64BIT:
	    dbgfmt_dwarf2->sizeof_offset = 8;
	    break;
    }
    dbgfmt_dwarf2->min_insn_len = yasm_arch_min_insn_len(a);

    return (yasm_dbgfmt *)dbgfmt_dwarf2;
}

static void
dwarf2_dbgfmt_destroy(/*@only@*/ yasm_dbgfmt *dbgfmt)
{
    yasm_dbgfmt_dwarf2 *dbgfmt_dwarf2 = (yasm_dbgfmt_dwarf2 *)dbgfmt;
    size_t i;
    for (i=0; i<dbgfmt_dwarf2->dirs_size; i++)
	if (dbgfmt_dwarf2->dirs[i])
	    yasm_xfree(dbgfmt_dwarf2->dirs[i]);
    yasm_xfree(dbgfmt_dwarf2->dirs);
    for (i=0; i<dbgfmt_dwarf2->filenames_size; i++)
	if (dbgfmt_dwarf2->filenames[i].filename)
	    yasm_xfree(dbgfmt_dwarf2->filenames[i].filename);
    yasm_xfree(dbgfmt_dwarf2->filenames);
    yasm_xfree(dbgfmt);
}

/* Add a bytecode to a section, updating offset on insertion;
 * no optimization necessary.
 */
static void
dwarf2_dbgfmt_append_bc(yasm_section *sect, yasm_bytecode *bc)
{
    yasm_bytecode *precbc = yasm_section_bcs_last(sect);
    bc->offset = precbc ? precbc->offset + precbc->len : 0;
    yasm_section_bcs_append(sect, bc);
}

/* Create and add a new line opcode to a section, updating offset on insertion;
 * no optimization necessary.
 */
static yasm_bytecode *
dwarf2_dbgfmt_append_line_op(yasm_section *sect, dwarf_line_number_op opcode,
			     /*@only@*/ /*@null@*/ yasm_intnum *operand)
{
    dwarf2_line_op *line_op = yasm_xmalloc(sizeof(dwarf2_line_op));
    yasm_bytecode *bc;

    line_op->opcode = opcode;
    line_op->operand = operand;
    line_op->ext_opcode = 0;
    line_op->ext_operand = NULL;
    line_op->ext_operandsize = 0;

    bc = yasm_bc_create_common(&dwarf2_line_op_bc_callback, line_op, 0);
    bc->len = 1;
    if (operand)
	bc->len += yasm_intnum_size_leb128(operand,
					   opcode == DW_LNS_advance_line);

    dwarf2_dbgfmt_append_bc(sect, bc);
    return bc;
}

/* Create and add a new extended line opcode to a section, updating offset on
 * insertion; no optimization necessary.
 */
static yasm_bytecode *
dwarf2_dbgfmt_append_line_ext_op(yasm_section *sect,
				 dwarf_line_number_ext_op ext_opcode,
				 unsigned long ext_operandsize,
				 /*@only@*/ /*@null@*/ yasm_expr *ext_operand)
{
    dwarf2_line_op *line_op = yasm_xmalloc(sizeof(dwarf2_line_op));
    yasm_bytecode *bc;

    line_op->opcode = DW_LNS_extended_op;
    line_op->operand = yasm_intnum_create_uint(ext_operandsize+1);
    line_op->ext_opcode = ext_opcode;
    line_op->ext_operand = ext_operand;
    line_op->ext_operandsize = ext_operandsize;

    bc = yasm_bc_create_common(&dwarf2_line_op_bc_callback, line_op, 0);
    bc->len = 2 + yasm_intnum_size_leb128(line_op->operand, 0) +
	ext_operandsize;

    dwarf2_dbgfmt_append_bc(sect, bc);
    return bc;
}

static void
dwarf2_dbgfmt_finalize_locs(yasm_section *sect, dwarf2_section_data *dsd)
{
    /*@dependent@*/ yasm_symrec *lastsym = NULL;
    /*@null@*/ yasm_bytecode *bc;
    /*@null@*/ dwarf2_loc *loc;

    bc = yasm_section_bcs_first(sect);
    STAILQ_FOREACH(loc, &dsd->locs, link) {
	/* Find the first bytecode following this loc by looking at
	 * the virtual line numbers.  XXX: this assumes the source file
	 * order will be the same as the actual section order.  If we ever
	 * implement subsegs this will NOT necessarily be true and this logic
	 * will need to be fixed to handle it!
	 *
	 * Keep track of last symbol seen prior to the loc.
	 */
	while (bc && bc->line <= loc->vline) {
	    if (bc->symrecs) {
		int i = 0;
		while (bc->symrecs[i]) {
		    lastsym = bc->symrecs[i];
		    i++;
		}
	    }
	    bc = yasm_bc__next(bc);
	}
	loc->sym = lastsym;
	loc->bc = bc;
    }
}

static int
dwarf2_dbgfmt_gen_line_op(yasm_section *debug_line, dwarf2_line_state *state,
			  dwarf2_loc *loc, /*@null@*/ dwarf2_loc *nextloc)
{
    unsigned long addr_delta;
    long line_delta;
    int opcode1, opcode2;
    yasm_dbgfmt_dwarf2 *dbgfmt_dwarf2 = state->dbgfmt_dwarf2;

    if (state->file != loc->file) {
	state->file = loc->file;
	dwarf2_dbgfmt_append_line_op(debug_line, DW_LNS_set_file,
				     yasm_intnum_create_uint(state->file));
    }
    if (state->column != loc->column) {
	state->column = loc->column;
	dwarf2_dbgfmt_append_line_op(debug_line, DW_LNS_set_column,
				     yasm_intnum_create_uint(state->column));
    }
#ifdef WITH_DWARF3
    if (loc->isa_change) {
	state->isa = loc->isa;
	dwarf2_dbgfmt_append_line_op(debug_line, DW_LNS_set_isa,
				     yasm_intnum_create_uint(state->isa));
    }
#endif
    if (state->is_stmt == 0 && loc->is_stmt == IS_STMT_SET) {
	state->is_stmt = 1;
	dwarf2_dbgfmt_append_line_op(debug_line, DW_LNS_negate_stmt, NULL);
    } else if (state->is_stmt == 1 && loc->is_stmt == IS_STMT_CLEAR) {
	state->is_stmt = 0;
	dwarf2_dbgfmt_append_line_op(debug_line, DW_LNS_negate_stmt, NULL);
    }
    if (loc->basic_block) {
	dwarf2_dbgfmt_append_line_op(debug_line, DW_LNS_set_basic_block, NULL);
    }
#ifdef WITH_DWARF3
    if (loc->prologue_end) {
	dwarf2_dbgfmt_append_line_op(debug_line, DW_LNS_set_prologue_end, NULL);
    }
    if (loc->epilogue_begin) {
	dwarf2_dbgfmt_append_line_op(debug_line, DW_LNS_set_epilogue_begin,
				     NULL);
    }
#endif

    /* If multiple loc for the same location, use last */
    if (nextloc && nextloc->bc->offset == loc->bc->offset)
	return 0;

    if (!state->precbc) {
	/* Set the starting address for the section */
	if (!loc->sym) {
	    /* shouldn't happen! */
	    yasm__error(loc->line, N_("could not find label prior to loc"));
	    return 1;
	}
	dwarf2_dbgfmt_append_line_ext_op(debug_line, DW_LNE_set_address,
	    dbgfmt_dwarf2->sizeof_address,
	    yasm_expr_create_ident(yasm_expr_sym(loc->sym), loc->line));
	addr_delta = 0;
    } else if (loc->bc) {
	if (state->precbc->offset > loc->bc->offset)
	    yasm_internal_error(N_("dwarf2 address went backwards?"));
	addr_delta = loc->bc->offset - state->precbc->offset;
    } else
	return 0;	/* ran out of bytecodes!  XXX: do something? */

    /* Generate appropriate opcode(s).  Address can only increment,
     * whereas line number can go backwards.
     */
    line_delta = loc->line - state->line;
    state->line = loc->line;

    /* First handle the line delta */
    if (line_delta < DWARF2_LINE_BASE
	|| line_delta >= DWARF2_LINE_BASE+DWARF2_LINE_RANGE) {
	/* Won't fit in special opcode, use (signed) line advance */
	dwarf2_dbgfmt_append_line_op(debug_line, DW_LNS_advance_line,
				     yasm_intnum_create_int(line_delta));
	line_delta = 0;
    }

    /* Next handle the address delta */
    opcode1 = DWARF2_LINE_OPCODE_BASE + line_delta - DWARF2_LINE_BASE +
	DWARF2_LINE_RANGE * (addr_delta / dbgfmt_dwarf2->min_insn_len);
    opcode2 = DWARF2_LINE_OPCODE_BASE + line_delta - DWARF2_LINE_BASE +
	DWARF2_LINE_RANGE * ((addr_delta - DWARF2_MAX_SPECIAL_ADDR_DELTA) /
			     dbgfmt_dwarf2->min_insn_len);
    if (line_delta == 0 && addr_delta == 0) {
	/* Both line and addr deltas are 0: do DW_LNS_copy */
	dwarf2_dbgfmt_append_line_op(debug_line, DW_LNS_copy, NULL);
    } else if (addr_delta <= DWARF2_MAX_SPECIAL_ADDR_DELTA && opcode1 <= 255) {
	/* Addr delta in range of special opcode */
	dwarf2_dbgfmt_append_line_op(debug_line, opcode1, NULL);
    } else if (addr_delta <= 2*DWARF2_MAX_SPECIAL_ADDR_DELTA
	       && opcode2 <= 255) {
	/* Addr delta in range of const_add_pc + special */
	unsigned int opcode;
	dwarf2_dbgfmt_append_line_op(debug_line, DW_LNS_const_add_pc, NULL);
	dwarf2_dbgfmt_append_line_op(debug_line, opcode2, NULL);
    } else {
	/* Need advance_pc */
	dwarf2_dbgfmt_append_line_op(debug_line, DW_LNS_advance_pc,
				     yasm_intnum_create_uint(addr_delta));
	/* Take care of any remaining line_delta and add entry to matrix */
	if (line_delta == 0)
	    dwarf2_dbgfmt_append_line_op(debug_line, DW_LNS_copy, NULL);
	else {
	    unsigned int opcode;
	    opcode = DWARF2_LINE_OPCODE_BASE + line_delta - DWARF2_LINE_BASE;
	    dwarf2_dbgfmt_append_line_op(debug_line, opcode, NULL);
	}
    }
    state->precbc = loc->bc;
    return 0;
}

static int
dwarf2_dbgfmt_generate_section(yasm_section *sect, /*@null@*/ void *d)
{
    dwarf2_info *info = (dwarf2_info *)d;
    yasm_dbgfmt_dwarf2 *dbgfmt_dwarf2 = info->dbgfmt_dwarf2;
    /*@null@*/ dwarf2_section_data *dsd;
    /*@null@*/ dwarf2_loc *loc;
    /*@null@*/ yasm_bytecode *bc;
    dwarf2_line_state state;
    unsigned long addr_delta;

    dsd = yasm_section_get_data(sect, &dwarf2_section_data_cb);
    if (!dsd)
	return 0;	/* no line data for this section */

    /* initialize state machine registers for each sequence */
    state.dbgfmt_dwarf2 = dbgfmt_dwarf2;
    state.address = 0;
    state.file = 1;
    state.line = 1;
    state.column = 0;
    state.isa = 0;
    state.is_stmt = DWARF2_LINE_DEFAULT_IS_STMT;
    state.precbc = NULL;

    dwarf2_dbgfmt_finalize_locs(sect, dsd);

    STAILQ_FOREACH(loc, &dsd->locs, link) {
	if (dwarf2_dbgfmt_gen_line_op(info->debug_line, &state, loc,
				      STAILQ_NEXT(loc, link)))
	    return 1;
    }

    /* End sequence: bring address to end of section, then output end
     * sequence opcode.  Don't use a special opcode to do this as we don't
     * want an extra entry in the line matrix.
     */
    bc = yasm_section_bcs_last(sect);
    addr_delta = bc->offset + bc->len - state.precbc->offset;
    if (addr_delta == DWARF2_MAX_SPECIAL_ADDR_DELTA)
	dwarf2_dbgfmt_append_line_op(info->debug_line, DW_LNS_const_add_pc,
				     NULL);
    else if (addr_delta > 0)
	dwarf2_dbgfmt_append_line_op(info->debug_line, DW_LNS_advance_pc,
				     yasm_intnum_create_uint(addr_delta));
    dwarf2_dbgfmt_append_line_ext_op(info->debug_line, DW_LNE_end_sequence, 0,
				     NULL);

    return 0;
}

static void
dwarf2_dbgfmt_generate(yasm_dbgfmt *dbgfmt)
{
    yasm_dbgfmt_dwarf2 *dbgfmt_dwarf2 = (yasm_dbgfmt_dwarf2 *)dbgfmt;
    dwarf2_info info;
    int new;
    size_t i;
    yasm_bytecode *last, *sppbc;
    dwarf2_spp *spp;
    yasm_intnum *cval;

    /* Only generate line info for now */

    info.dbgfmt_dwarf2 = dbgfmt_dwarf2;
    info.debug_line = yasm_object_get_general(dbgfmt_dwarf2->object,
					      ".debug_line", 0, 4, 0, 0, &new,
					      0);
    yasm_section_set_align(info.debug_line, 0, 0);
    last = yasm_section_bcs_last(info.debug_line);

    /* statement program prologue */
    spp = yasm_xmalloc(sizeof(dwarf2_spp));
    spp->dbgfmt_dwarf2 = dbgfmt_dwarf2;
    spp->line_start_prevbc = last;
    sppbc = yasm_bc_create_common(&dwarf2_spp_bc_callback, spp, 0);
    sppbc->offset = last->offset + last->len;
    sppbc->len = dbgfmt_dwarf2->sizeof_offset*2 + 2 + 5 +
	NELEMS(line_opcode_num_operands);
    if (dbgfmt_dwarf2->format == DWARF2_FORMAT_64BIT)
	sppbc->len += 4;
    /* directory list */
    for (i=0; i<dbgfmt_dwarf2->dirs_size; i++)
	sppbc->len += strlen(dbgfmt_dwarf2->dirs[i])+1;
    sppbc->len++;
    /* filename list */
    cval = yasm_intnum_create_uint(0);
    for (i=0; i<dbgfmt_dwarf2->filenames_size; i++) {
	if (!dbgfmt_dwarf2->filenames[i].filename)
	    yasm__error(0, N_("dwarf2 file number %d unassigned"), i+1);
	yasm_intnum_set_uint(cval, dbgfmt_dwarf2->filenames[i].dir);
	sppbc->len += strlen(dbgfmt_dwarf2->filenames[i].filename) + 1 +
	    yasm_intnum_size_leb128(cval, 0) + 2;
    }
    yasm_intnum_destroy(cval);
    sppbc->len++;
    yasm_section_bcs_append(info.debug_line, sppbc);

    /* statement program */
    yasm_object_sections_traverse(dbgfmt_dwarf2->object, (void *)&info,
				  dwarf2_dbgfmt_generate_section);

    /* fill initial pseudo-stab's fields */
    spp->line_end_prevbc = yasm_section_bcs_last(info.debug_line);
}

static void
dwarf2_section_data_destroy(void *data)
{
    dwarf2_section_data *dsd = data;
    dwarf2_loc *n1, *n2;

    /* Delete locations */
    n1 = STAILQ_FIRST(&dsd->locs);
    while (n1) {
	n2 = STAILQ_NEXT(n1, link);
	yasm_xfree(n1);
	n1 = n2;
    }

    yasm_xfree(data);
}

static void
dwarf2_section_data_print(void *data, FILE *f, int indent_level)
{
    /* TODO */
}

static void
dwarf2_spp_bc_destroy(void *contents)
{
    yasm_xfree(contents);
}

static void
dwarf2_spp_bc_print(const void *contents, FILE *f, int indent_level)
{
    /* TODO */
}

static yasm_bc_resolve_flags
dwarf2_spp_bc_resolve(yasm_bytecode *bc, int save,
		      yasm_calc_bc_dist_func calc_bc_dist)
{
    yasm_internal_error(N_("tried to resolve a dwarf2 spp bytecode"));
    /*@notreached@*/
    return YASM_BC_RESOLVE_MIN_LEN;
}

static int
dwarf2_spp_bc_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		      yasm_output_expr_func output_expr,
		      yasm_output_reloc_func output_reloc)
{
    dwarf2_spp *spp = (dwarf2_spp *)bc->contents;
    yasm_dbgfmt_dwarf2 *dbgfmt_dwarf2 = spp->dbgfmt_dwarf2;
    unsigned char *buf = *bufp;
    yasm_intnum *intn, *cval;
    size_t i, len;

    if (dbgfmt_dwarf2->format == DWARF2_FORMAT_64BIT) {
	YASM_WRITE_8(buf, 0xff);
	YASM_WRITE_8(buf, 0xff);
	YASM_WRITE_8(buf, 0xff);
	YASM_WRITE_8(buf, 0xff);
    }
    /* Total length of statement info (following this field) */
    cval = yasm_intnum_create_uint(dbgfmt_dwarf2->sizeof_offset);
    intn = yasm_common_calc_bc_dist(spp->line_start_prevbc,
				    spp->line_end_prevbc);
    yasm_intnum_calc(intn, YASM_EXPR_SUB, cval, bc->line);
    yasm_arch_intnum_tobytes(dbgfmt_dwarf2->arch, intn, buf,
			     dbgfmt_dwarf2->sizeof_offset,
			     dbgfmt_dwarf2->sizeof_offset*8, 0, bc, 0, 0);
    buf += dbgfmt_dwarf2->sizeof_offset;
    yasm_intnum_destroy(intn);

    /* DWARF version */
    yasm_intnum_set_uint(cval, 2);
    yasm_arch_intnum_tobytes(dbgfmt_dwarf2->arch, cval, buf, 2, 16, 0, bc, 0,
			     0);
    buf += 2;

    /* Prologue length (following this field) */
    cval = yasm_intnum_create_uint(bc->len - (buf-*bufp) -
				   dbgfmt_dwarf2->sizeof_offset);
    yasm_arch_intnum_tobytes(dbgfmt_dwarf2->arch, cval, buf,
			     dbgfmt_dwarf2->sizeof_offset,
			     dbgfmt_dwarf2->sizeof_offset*8, 0, bc,
			     0, 0);
    buf += dbgfmt_dwarf2->sizeof_offset;

    YASM_WRITE_8(buf, dbgfmt_dwarf2->min_insn_len);	/* minimum_instr_len */
    YASM_WRITE_8(buf, DWARF2_LINE_DEFAULT_IS_STMT);	/* default_is_stmt */
    YASM_WRITE_8(buf, DWARF2_LINE_BASE);		/* line_base */
    YASM_WRITE_8(buf, DWARF2_LINE_RANGE);		/* line_range */
    YASM_WRITE_8(buf, DWARF2_LINE_OPCODE_BASE);		/* opcode_base */

    /* Standard opcode # operands array */
    for (i=0; i<NELEMS(line_opcode_num_operands); i++)
	YASM_WRITE_8(buf, line_opcode_num_operands[i]);

    /* directory list */
    for (i=0; i<dbgfmt_dwarf2->dirs_size; i++) {
	len = strlen(dbgfmt_dwarf2->dirs[i])+1;
	memcpy(buf, dbgfmt_dwarf2->dirs[i], len);
	buf += len;
    }
    /* finish with single 0 byte */
    YASM_WRITE_8(buf, 0);

    /* filename list */
    for (i=0; i<dbgfmt_dwarf2->filenames_size; i++) {
	len = strlen(dbgfmt_dwarf2->filenames[i].filename)+1;
	memcpy(buf, dbgfmt_dwarf2->filenames[i].filename, len);
	buf += len;

	/* dir */
	yasm_intnum_set_uint(cval, dbgfmt_dwarf2->filenames[i].dir);
	buf += yasm_intnum_get_leb128(cval, buf, 0);
	YASM_WRITE_8(buf, 0);	/* time */
	YASM_WRITE_8(buf, 0);	/* length */
    }
    /* finish with single 0 byte */
    YASM_WRITE_8(buf, 0);

    *bufp = buf;

    yasm_intnum_destroy(cval);
    return 0;
}

static void
dwarf2_line_op_bc_destroy(void *contents)
{
    dwarf2_line_op *line_op = (dwarf2_line_op *)contents;
    if (line_op->operand)
	yasm_intnum_destroy(line_op->operand);
    if (line_op->ext_operand)
	yasm_expr_destroy(line_op->ext_operand);
    yasm_xfree(contents);
}

static void
dwarf2_line_op_bc_print(const void *contents, FILE *f, int indent_level)
{
}

static yasm_bc_resolve_flags
dwarf2_line_op_bc_resolve(yasm_bytecode *bc, int save,
			  yasm_calc_bc_dist_func calc_bc_dist)
{
    yasm_internal_error(N_("tried to resolve a dwarf2 line_op bytecode"));
    /*@notreached@*/
    return YASM_BC_RESOLVE_MIN_LEN;
}

static int
dwarf2_line_op_bc_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
			  yasm_output_expr_func output_expr,
			  yasm_output_reloc_func output_reloc)
{
    dwarf2_line_op *line_op = (dwarf2_line_op *)bc->contents;
    unsigned char *buf = *bufp;

    YASM_WRITE_8(buf, line_op->opcode);
    if (line_op->operand)
	buf += yasm_intnum_get_leb128(line_op->operand, buf,
				      line_op->opcode == DW_LNS_advance_line);
    if (line_op->ext_opcode > 0) {
	YASM_WRITE_8(buf, line_op->ext_opcode);
	if (line_op->ext_operand) {
	    output_expr(&line_op->ext_operand, buf, line_op->ext_operandsize,
			line_op->ext_operandsize*8, 0,
			(unsigned long)(buf-*bufp), bc, 0, 0, d);
	    buf += line_op->ext_operandsize;
	}
    }

    *bufp = buf;
    return 0;
}

static int
dwarf2_dbgfmt_directive(yasm_dbgfmt *dbgfmt, const char *name,
			yasm_section *sect, yasm_valparamhead *valparams,
			unsigned long line)
{
    yasm_dbgfmt_dwarf2 *dbgfmt_dwarf2 = (yasm_dbgfmt_dwarf2 *)dbgfmt;

    if (yasm__strcasecmp(name, "loc") == 0) {
	/*@dependent@*/ /*@null@*/ const yasm_intnum *intn;
	dwarf2_section_data *dsd;
	dwarf2_loc *loc = yasm_xmalloc(sizeof(dwarf2_loc));

	/* File number (required) */
	yasm_valparam *vp = yasm_vps_first(valparams);
	if (!vp || !vp->param) {
	    yasm__error(line, N_("file number required"));
	    yasm_xfree(loc);
	    return 0;
	}
	intn = yasm_expr_get_intnum(&vp->param, NULL);
	if (!intn) {
	    yasm__error(line, N_("file number is not a constant"));
	    yasm_xfree(loc);
	    return 0;
	}
	if (yasm_intnum_sign(intn) != 1) {
	    yasm__error(line, N_("file number less than one"));
	    yasm_xfree(loc);
	    return 0;
	}
	loc->file = yasm_intnum_get_uint(intn);

	/* Line number (required) */
	vp = yasm_vps_next(vp);
	if (!vp || !vp->param) {
	    yasm__error(line, N_("line number required"));
	    yasm_xfree(loc);
	    return 0;
	}
	intn = yasm_expr_get_intnum(&vp->param, NULL);
	if (!intn) {
	    yasm__error(line, N_("file number is not a constant"));
	    yasm_xfree(loc);
	    return 0;
	}
	loc->line = yasm_intnum_get_uint(intn);

	/* Generate new section data if it doesn't already exist */
	dsd = yasm_section_get_data(sect, &dwarf2_section_data_cb);
	if (!dsd) {
	    dsd = yasm_xmalloc(sizeof(dwarf2_section_data));
	    STAILQ_INIT(&dsd->locs);
	    yasm_section_add_data(sect, &dwarf2_section_data_cb, dsd);
	}

	/* Defaults for optional settings */
	loc->column = 0;
	loc->isa_change = 0;
	loc->isa = 0;
	loc->is_stmt = IS_STMT_NOCHANGE;
	loc->basic_block = 0;
	loc->prologue_end = 0;
	loc->epilogue_begin = 0;

	/* Optional column number */
	vp = yasm_vps_next(vp);
	if (vp && vp->param) {
	    intn = yasm_expr_get_intnum(&vp->param, NULL);
	    if (!intn) {
		yasm__error(line, N_("column number is not a constant"));
		yasm_xfree(loc);
		return 0;
	    }
	    loc->column = yasm_intnum_get_uint(intn);
	    vp = yasm_vps_next(vp);
	}

	/* Other options */
	while (vp && vp->val) {
	    if (yasm__strcasecmp(vp->val, "basic_block") == 0)
		loc->basic_block = 1;
	    else if (yasm__strcasecmp(vp->val, "prologue_end") == 0)
		loc->prologue_end = 1;
	    else if (yasm__strcasecmp(vp->val, "epilogue_begin") == 0)
		loc->epilogue_begin = 1;
	    else if (yasm__strcasecmp(vp->val, "is_stmt") == 0) {
		if (!vp->param) {
		    yasm__error(line, N_("is_stmt requires value"));
		    yasm_xfree(loc);
		    return 0;
		}
		intn = yasm_expr_get_intnum(&vp->param, NULL);
		if (!intn) {
		    yasm__error(line, N_("is_stmt value is not a constant"));
		    yasm_xfree(loc);
		    return 0;
		}
		if (yasm_intnum_is_zero(intn))
		    loc->is_stmt = IS_STMT_SET;
		else if (yasm_intnum_is_pos1(intn))
		    loc->is_stmt = IS_STMT_CLEAR;
		else {
		    yasm__error(line, N_("is_stmt value not 0 or 1"));
		    yasm_xfree(loc);
		    return 0;
		}
	    } else if (yasm__strcasecmp(vp->val, "isa") == 0) {
		if (!vp->param) {
		    yasm__error(line, N_("isa requires value"));
		    yasm_xfree(loc);
		    return 0;
		}
		intn = yasm_expr_get_intnum(&vp->param, NULL);
		if (!intn) {
		    yasm__error(line, N_("isa value is not a constant"));
		    yasm_xfree(loc);
		    return 0;
		}
		if (yasm_intnum_sign(intn) < 0) {
		    yasm__error(line, N_("isa value less than zero"));
		    yasm_xfree(loc);
		    return 0;
		}
		loc->isa_change = 1;
		loc->isa = yasm_intnum_get_uint(intn);
	    } else
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("unrecognized loc option `%s'"), vp->val);
	}

	/* Append new location */
	loc->vline = line;
	loc->bc = NULL;
	loc->sym = NULL;
	STAILQ_INSERT_TAIL(&dsd->locs, loc, link);

	return 0;
    } else if (yasm__strcasecmp(name, "file") == 0) {
	/*@dependent@*/ /*@null@*/ const yasm_intnum *file_intn;
	size_t filenum;
	size_t dirlen;
	const char *filename;
	size_t i, dir;

	yasm_valparam *vp = yasm_vps_first(valparams);

	if (vp->val) {
	    /* Just a bare filename */
	    yasm_object_set_source_fn(dbgfmt_dwarf2->object, vp->val);
	    return 0;
	}

	/* Otherwise.. first vp is the file number */
	file_intn = yasm_expr_get_intnum(&vp->param, NULL);
	if (!file_intn) {
	    yasm__error(line, N_("file number is not a constant"));
	    return 0;
	}
	filenum = (size_t)yasm_intnum_get_uint(file_intn);

	vp = yasm_vps_next(vp);
	if (!vp || !vp->val) {
	    yasm__error(line, N_("file number given but no filename"));
	    return 0;
	}

	/* Put the directory into the directory table */
	dir = 0;
	dirlen = yasm__splitpath(vp->val, &filename);
	if (dirlen > 0) {
	    /* Look to see if we already have that dir in the table */
	    for (dir=1; dir<dbgfmt_dwarf2->dirs_size+1; dir++) {
		if (strncmp(dbgfmt_dwarf2->dirs[dir-1], vp->val, dirlen) == 0
		    && dbgfmt_dwarf2->dirs[dir-1][dirlen] == '\0')
		    break;
	    }
	    if (dir >= dbgfmt_dwarf2->dirs_size+1) {
		/* Not found in table, add to end, reallocing if necessary */
		if (dir >= dbgfmt_dwarf2->dirs_allocated+1) {
		    dbgfmt_dwarf2->dirs_allocated = dir+32;
		    dbgfmt_dwarf2->dirs = yasm_xrealloc(dbgfmt_dwarf2->dirs,
			sizeof(char *)*dbgfmt_dwarf2->dirs_allocated);
		}
		dbgfmt_dwarf2->dirs[dir-1] = yasm__xstrndup(vp->val, dirlen);
		dbgfmt_dwarf2->dirs_size = dir;
	    }
	}

	/* Put the filename into the filename table */
	if (filenum == 0) {
	    /* Look to see if we already have that filename in the table */
	    for (; filenum<dbgfmt_dwarf2->filenames_size; filenum++) {
		if (!dbgfmt_dwarf2->filenames[filenum].filename ||
		    (dbgfmt_dwarf2->filenames[filenum].dir == dir
		     && strcmp(dbgfmt_dwarf2->filenames[filenum].filename,
			       filename) == 0))
		    break;
	    }
	} else
	    filenum--;	/* array index is 0-based */

	/* Realloc table if necessary */
	if (filenum >= dbgfmt_dwarf2->filenames_allocated) {
	    size_t old_allocated = dbgfmt_dwarf2->filenames_allocated;
	    dbgfmt_dwarf2->filenames_allocated = filenum+32;
	    dbgfmt_dwarf2->filenames = yasm_xrealloc(dbgfmt_dwarf2->filenames,
		sizeof(dwarf2_filename)*dbgfmt_dwarf2->filenames_allocated);
	    for (i=old_allocated; i<dbgfmt_dwarf2->filenames_allocated; i++) {
		dbgfmt_dwarf2->filenames[i].filename = NULL;
		dbgfmt_dwarf2->filenames[i].dir = 0;
	    }
	}

	/* Actually save in table */
	if (dbgfmt_dwarf2->filenames[filenum].filename)
	    yasm_xfree(dbgfmt_dwarf2->filenames[filenum].filename);
	dbgfmt_dwarf2->filenames[filenum].filename = yasm__xstrdup(filename);
	dbgfmt_dwarf2->filenames[filenum].dir = dir;

	/* Update table size */
	if (filenum >= dbgfmt_dwarf2->filenames_size)
	    dbgfmt_dwarf2->filenames_size = filenum + 1;

	return 0;
    }
    return 1;
}

/* Define dbgfmt structure -- see dbgfmt.h for details */
yasm_dbgfmt_module yasm_dwarf2_LTX_dbgfmt = {
    "DWARF2 debugging format",
    "dwarf2",
    dwarf2_dbgfmt_create,
    dwarf2_dbgfmt_destroy,
    dwarf2_dbgfmt_directive,
    dwarf2_dbgfmt_generate
};
