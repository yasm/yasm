/* $IdPath$
 * YASM architecture interface header file
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
#ifndef YASM_ARCH_H
#define YASM_ARCH_H

typedef enum yasm_arch_check_id_retval {
    YASM_ARCH_CHECK_ID_NONE = 0,	/* just a normal identifier */
    YASM_ARCH_CHECK_ID_INSN,		/* an instruction */
    YASM_ARCH_CHECK_ID_PREFIX,		/* an instruction prefix */ 
    YASM_ARCH_CHECK_ID_REG,		/* a register */
    YASM_ARCH_CHECK_ID_SEGREG,	/* a segment register (for memory overrides) */
    YASM_ARCH_CHECK_ID_TARGETMOD	/* an target modifier (for jumps) */
} yasm_arch_check_id_retval;

typedef struct yasm_insn_operand yasm_insn_operand;
typedef struct yasm_insn_operandhead yasm_insn_operandhead;
#ifdef YASM_INTERNAL
/*@reldef@*/ STAILQ_HEAD(yasm_insn_operandhead, yasm_insn_operand);
#endif

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
typedef enum yasm_arch_syntax_flavor {
    YASM_ARCH_SYNTAX_FLAVOR_NASM = 1,	/* like NASM */
    YASM_ARCH_SYNTAX_FLAVOR_GAS		/* like GAS */
} yasm_arch_syntax_flavor;

struct yasm_arch {
    /* one-line description of the architecture */
    const char *name;

    /* keyword used to select architecture */
    const char *keyword;

    void (*initialize) (void);
    void (*cleanup) (void);

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
	void (*switch_cpu) (const char *cpuid, unsigned long lindex);

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
	yasm_arch_check_id_retval (*check_identifier)
	    (unsigned long data[4], const char *id, unsigned long lindex);

	/* Architecture-specific directive support.  Returns 1 if directive was
	 * not recognized.  Returns 0 if directive was recognized, even if it
	 * wasn't valid.  Should modify behavior ONLY of parse functions, much
	 * like switch_cpu() above.
	 */
	int (*directive) (const char *name, yasm_valparamhead *valparams,
			  /*@null@*/ yasm_valparamhead *objext_valparams,
			  yasm_sectionhead *headp, unsigned long lindex);

	/* Creates an instruction.  Creates a bytecode by matching the
	 * instruction data and the parameters given with a valid instruction.
	 * If no match is found (the instruction is invalid), returns NULL.
	 * All zero data indicates an empty instruction should be created.
	 */
	/*@null@*/ yasm_bytecode * (*new_insn)
	    (const unsigned long data[4], int num_operands,
	     /*@null@*/ yasm_insn_operandhead *operands,
	     yasm_section *cur_section, /*@null@*/ yasm_bytecode *prev_bc,
	     unsigned long lindex);

	/* Handle an instruction prefix by modifying bc as necessary. */
	void (*handle_prefix) (yasm_bytecode *bc, const unsigned long data[4],
			       unsigned long lindex);

	/* Handle an segment register instruction prefix by modifying bc as
	 * necessary.
	 */
	void (*handle_seg_prefix) (yasm_bytecode *bc, unsigned long segreg,
				   unsigned long lindex);

	/* Handle memory expression segment overrides by modifying ea as
	 * necessary.
	 */
	void (*handle_seg_override) (yasm_effaddr *ea, unsigned long segreg,
				     unsigned long lindex);

	/* Convert an expression into an effective address. */
	yasm_effaddr * (*ea_new_expr) (/*@keep@*/ yasm_expr *e);
    } parse;

    struct {
	/* Maximum used bytecode type value+1.  Should be set to
	 * BYTECODE_TYPE_BASE if no additional bytecode types are defined by
	 * the architecture.
	 */
	const int type_max;

	void (*bc_delete) (yasm_bytecode *bc);
	void (*bc_print) (FILE *f, int indent_level, const yasm_bytecode *bc);

	/* See bytecode.h comments on bc_resolve() */
	yasm_bc_resolve_flags (*bc_resolve)
	    (yasm_bytecode *bc, int save, const yasm_section *sect,
	     yasm_calc_bc_dist_func calc_bc_dist);
	/* See bytecode.h comments on bc_tobytes() */
	int (*bc_tobytes) (yasm_bytecode *bc, unsigned char **bufp,
			   const yasm_section *sect, void *d,
			   yasm_output_expr_func output_expr);
    } bc;

    /* Functions to output floats and integers, architecture-specific because
     * of endianness.  Returns nonzero on error, otherwise updates bufp by
     * valsize (bytes saved to bufp).  For intnums, rel indicates a relative
     * displacement, and bc is the containing bytecode to compute it from.
     */
    int (*floatnum_tobytes) (const yasm_floatnum *flt, unsigned char **bufp,
			     unsigned long valsize, const yasm_expr *e);
    int (*intnum_tobytes) (const yasm_intnum *intn, unsigned char **bufp,
			   unsigned long valsize, const yasm_expr *e,
			   const yasm_bytecode *bc, int rel);

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
    void (*ea_data_delete) (yasm_effaddr *ea);

    void (*ea_data_print) (FILE *f, int indent_level, const yasm_effaddr *ea);
};

#ifdef YASM_INTERNAL
struct yasm_insn_operand {
    /*@reldef@*/ STAILQ_ENTRY(yasm_insn_operand) link;

    enum {
	YASM_INSN__OPERAND_REG = 1,	/* a register */
	YASM_INSN__OPERAND_SEGREG,	/* a segment register */
	YASM_INSN__OPERAND_MEMORY,/* an effective address (memory reference) */
	YASM_INSN__OPERAND_IMM		/* an immediate or jump target */
    } type;

    union {
	unsigned long reg;	/* arch data for reg/segreg */
	yasm_effaddr *ea;	/* effective address for memory references */
	yasm_expr *val;		/* value of immediate or jump target */
    } data;

    unsigned long targetmod;	/* arch target modifier, 0 if none */

    /* Specified size of the operand, in bytes.  0 if not user-specified. */
    unsigned int size;
};
#endif

void yasm_arch_common_initialize(yasm_arch *a);

/* insn_operand constructors.  operand_new_imm() will look for cases of a
 * single register and create an INSN_OPERAND_REG variant of insn_operand.
 */
yasm_insn_operand *yasm_operand_new_reg(unsigned long reg);
yasm_insn_operand *yasm_operand_new_segreg(unsigned long segreg);
yasm_insn_operand *yasm_operand_new_mem(/*@only@*/ yasm_effaddr *ea);
yasm_insn_operand *yasm_operand_new_imm(/*@only@*/ yasm_expr *val);

void yasm_operand_print(FILE *f, int indent_level,
			const yasm_insn_operand *op);

void yasm_ops_initialize(yasm_insn_operandhead *headp);
yasm_insn_operand *yasm_ops_first(yasm_insn_operandhead *headp);
yasm_insn_operand *yasm_ops_next(yasm_insn_operand *cur);
#ifdef YASM_INTERNAL
#define yasm_ops_initialize(headp)	STAILQ_INIT(headp)
#define yasm_ops_first(headp)		STAILQ_FIRST(headp)
#define yasm_ops_next(cur)		STAILQ_NEXT(cur, link)
#endif

/* Deletes operands linked list.  Deletes content of each operand if content i
 * nonzero.
 */
void yasm_ops_delete(yasm_insn_operandhead *headp, int content);

/* Adds op to the list of operands headp.
 * NOTE: Does not make a copy of op; so don't pass this function
 * static or local variables, and discard the op pointer after calling
 * this function.  If op was actually appended (it wasn't NULL), then
 * returns op, otherwise returns NULL.
 */
/*@null@*/ yasm_insn_operand *yasm_ops_append
    (yasm_insn_operandhead *headp,
     /*@returned@*/ /*@null@*/ yasm_insn_operand *op);

void yasm_ops_print(FILE *f, int indent_level,
		    const yasm_insn_operandhead *headp);

#endif
