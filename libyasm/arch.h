/* $IdPath$
 * Architecture header file
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
#ifndef YASM_ARCH_H
#define YASM_ARCH_H

typedef enum arch_check_id_retval {
    ARCH_CHECK_ID_NONE = 0,	/* just a normal identifier */
    ARCH_CHECK_ID_INSN,		/* an instruction */
    ARCH_CHECK_ID_PREFIX,	/* an instruction prefix */ 
    ARCH_CHECK_ID_REG,		/* a register */
    ARCH_CHECK_ID_SEGREG,	/* a segment register (for memory overrides) */
    ARCH_CHECK_ID_TARGETMOD	/* an target modifier (for jumps) */
} arch_check_id_retval;

typedef /*@reldef@*/ STAILQ_HEAD(insn_operandhead, insn_operand)
	insn_operandhead;

typedef struct insn_operand insn_operand;

/* Different assemblers order instruction operands differently.  Also, some
 * differ on how exactly various registers are specified.  There's no great
 * solution to this, as the parsers aren't supposed to have knowledge of the
 * architectural internals, and the architecture is supposed to be parser-
 * independent.  To make things work, as a rather hackish solution, we give the
 * architecture a little knowledge about the general "flavor" of the parser,
 * and let the architecture decide what to do with it.  Most architectures will
 * probably not even use this, but it's required for some (x86 in particular)
 * for correct behavior on all parsers.
 */
typedef enum arch_syntax_flavor {
    ARCH_SYNTAX_FLAVOR_NASM = 1,	/* like NASM */
    ARCH_SYNTAX_FLAVOR_GAS		/* like GAS */
} arch_syntax_flavor;

struct arch {
    /* one-line description of the architecture */
    const char *name;

    /* keyword used to select architecture */
    const char *keyword;

    struct {
	/* All "data" below starts the parse initialized to 0.  Thus, it is
	 * okay for a funtion to use/check previously stored data to see if
	 * it's been called before on the same piece of data.
	 */

	/* Switches available instructions/registers/etc. based on a
	 * user-specified CPU identifier.  Should modify behavior ONLY of
	 * parse functions!  The bytecode and output functions should be able
	 * to handle any CPU.
	 */
	void (*switch_cpu) (const char *cpuid);

	/* Checks an generic identifier to see if it matches architecture
	 * specific names for instructions, registers, etc (see the
	 * arch_check_id_retval enum above for the various types this function
	 * can detect & return.  Unrecognized identifiers should be returned
	 * as NONE so they can be treated as normal symbols.  Any additional
	 * data beyond just the type (almost always necessary) should be
	 * returned into the space provided by the data parameter.
	 * Note: even though this is passed a data[4], only data[0] should be
	 * used for TARGETMOD, REG, and SEGREG return values.
	 */
	arch_check_id_retval (*check_identifier) (unsigned long data[4],
						  const char *id);

	/* Architecture-specific directive support.  Returns 1 if directive was
	 * not recognized.  Returns 0 if directive was recognized, even if it
	 * wasn't valid.  Should modify behavior ONLY of parse functions, much
	 * like switch_cpu() above.
	 */
	int (*directive) (const char *name, valparamhead *valparams,
			  /*@null@*/ valparamhead *objext_valparams,
			  sectionhead *headp);

	/* Creates an instruction.  Creates a bytecode by matching the
	 * instruction data and the parameters given with a valid instruction.
	 * If no match is found (the instruction is invalid), returns NULL.
	 * All zero data indicates an empty instruction should be created.
	 */
	/*@null@*/ bytecode * (*new_insn) (const unsigned long data[4],
					   int num_operands, /*@null@*/
					   insn_operandhead *operands,
					   section *cur_section,
					   /*@null@*/ bytecode *prev_bc);

	/* Handle an instruction prefix by modifying bc as necessary. */
	void (*handle_prefix) (bytecode *bc, const unsigned long data[4]);

	/* Handle an segment register instruction prefix by modifying bc as
	 * necessary.
	 */
	void (*handle_seg_prefix) (bytecode *bc, unsigned long segreg);

	/* Handle memory expression segment overrides by modifying ea as
	 * necessary.
	 */
	void (*handle_seg_override) (effaddr *ea, unsigned long segreg);

	/* Convert an expression into an effective address. */
	effaddr * (*ea_new_expr) (/*@keep@*/ expr *e);
    } parse;

    struct {
	/* Maximum used bytecode type value+1.  Should be set to
	 * BYTECODE_TYPE_BASE if no additional bytecode types are defined by
	 * the architecture.
	 */
	const int type_max;

	void (*bc_delete) (bytecode *bc);
	void (*bc_print) (FILE *f, const bytecode *bc);

	/* See bytecode.h comments on bc_resolve() */
	bc_resolve_flags (*bc_resolve) (bytecode *bc, int save,
					const section *sect,
					calc_bc_dist_func calc_bc_dist);
	/* See bytecode.h comments on bc_tobytes() */
	int (*bc_tobytes) (bytecode *bc, unsigned char **bufp,
			   const section *sect, void *d,
			   output_expr_func output_expr);
    } bc;

    /* Gets the equivalent register size in bytes.  Returns 0 if there is no
     * suitable equivalent size.
     */
    unsigned int (*get_reg_size) (unsigned long reg);

    void (*reg_print) (FILE *f, unsigned long reg);
    void (*segreg_print) (FILE *f, unsigned long segreg);

    /* Deletes the arch-specific data in ea.  May be NULL if no special
     * deletion is required (e.g. there's no dynamically allocated pointers
     * in the ea data).
     */
    void (*ea_data_delete) (effaddr *ea);

    void (*ea_data_print) (FILE *f, const effaddr *ea);
};

struct insn_operand {
    /*@reldef@*/ STAILQ_ENTRY(insn_operand) link;

    enum {
	INSN_OPERAND_REG = 1,	/* a register */
	INSN_OPERAND_SEGREG,	/* a segment register */
	INSN_OPERAND_MEMORY,	/* an effective address (memory reference) */
	INSN_OPERAND_IMM	/* an immediate or jump target */
    } type;

    union {
	unsigned long reg;	/* arch data for reg/segreg */
	effaddr *ea;		/* effective address for memory references */
	expr *val;		/* value of immediate or jump target */
    } data;

    unsigned long targetmod;	/* arch target modifier, 0 if none */

    /* Specified size of the operand, in bytes.  0 if not user-specified. */
    unsigned int size;
};

/* insn_operand constructors.  operand_new_imm() will look for cases of a
 * single register and create an INSN_OPERAND_REG variant of insn_operand.
 */
insn_operand *operand_new_reg(unsigned long reg);
insn_operand *operand_new_segreg(unsigned long segreg);
insn_operand *operand_new_mem(/*@only@*/ effaddr *ea);
insn_operand *operand_new_imm(/*@only@*/ expr *val);

void operand_print(FILE *f, const insn_operand *op);

#define ops_initialize(headp)	STAILQ_INIT(headp)
#define ops_first(headp)	STAILQ_FIRST(headp)
#define ops_next(cur)		STAILQ_NEXT(cur, link)

/* Deletes operands linked list.  Deletes content of each operand if content i
 * nonzero.
 */
void ops_delete(insn_operandhead *headp, int content);

/* Adds op to the list of operands headp.
 * NOTE: Does not make a copy of op; so don't pass this function
 * static or local variables, and discard the op pointer after calling
 * this function.  If op was actually appended (it wasn't NULL), then
 * returns op, otherwise returns NULL.
 */
/*@null@*/ insn_operand *ops_append(insn_operandhead *headp,
				    /*@returned@*/ /*@null@*/ insn_operand *op);

void ops_print(FILE *f, const insn_operandhead *headp);

/* Available architectures */
extern arch x86_arch;

extern arch *cur_arch;

#endif
