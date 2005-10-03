/**
 * \file libyasm/arch.h
 * \brief YASM architecture interface.
 *
 * \rcs
 * $Id$
 * \endrcs
 *
 * \license
 *  Copyright (C) 2002  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
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
 * \endlicense
 */
#ifndef YASM_ARCH_H
#define YASM_ARCH_H

/** Errors that may be returned by yasm_arch_module::create(). */
typedef enum {
    YASM_ARCH_CREATE_OK = 0,		/**< No error. */
    YASM_ARCH_CREATE_BAD_MACHINE,	/**< Unrecognized machine name. */
    YASM_ARCH_CREATE_BAD_PARSER		/**< Unrecognized parser name. */
} yasm_arch_create_error;

/** An instruction operand (opaque type). */
typedef struct yasm_insn_operand yasm_insn_operand;
#ifdef YASM_LIB_INTERNAL
/*@reldef@*/ STAILQ_HEAD(yasm_insn_operands, yasm_insn_operand);
#endif

#ifndef YASM_DOXYGEN
/** Base #yasm_arch structure.  Must be present as the first element in any
 * #yasm_arch implementation.
 */
typedef struct yasm_arch_base {
    /** #yasm_arch_module implementation for this architecture. */
    const struct yasm_arch_module *module;
} yasm_arch_base;
#endif

/** YASM machine subtype.  A number of different machine types may be
 * associated with a single architecture.  These may be specific CPU's, but
 * the ABI used to interface with the architecture should be the primary
 * differentiator between machines.  Some object formats (ELF) use the machine
 * to determine parameters within the generated output.
 */
typedef struct yasm_arch_machine {
    /** One-line description of the machine. */
    const char *name;

    /** Keyword used to select machine. */
    const char *keyword;
} yasm_arch_machine;

/** YASM architecture module interface.
 * \note All "data" in parser-related functions (yasm_arch_parse_*) needs to
 *	 start the parse initialized to 0 to make it okay for a parser-related
 *	 function to use/check previously stored data to see if it's been
 *	 called before on the same piece of data.
 */
typedef struct yasm_arch_module {
    /** One-line description of the architecture.
     * Call yasm_arch_name() to get the name of a particular #yasm_arch.
     */
    const char *name;

    /** Keyword used to select architecture.
     * Call yasm_arch_keyword() to get the keyword of a particular #yasm_arch.
     */
    const char *keyword;

    /** Create architecture.
     * Module-level implementation of yasm_arch_create().
     * Call yasm_arch_create() instead of calling this function.
     */
    /*@only@*/ yasm_arch * (*create) (const char *machine, const char *parser,
				      yasm_arch_create_error *error);

    /** Module-level implementation of yasm_arch_destroy().
     * Call yasm_arch_destroy() instead of calling this function.
     */
    void (*destroy) (/*@only@*/ yasm_arch *arch);

    /** Module-level implementation of yasm_arch_get_machine().
     * Call yasm_arch_get_machine() instead of calling this function.
     */
    const char * (*get_machine) (const yasm_arch *arch);

    /** Module-level implementation of yasm_arch_set_var().
     * Call yasm_arch_set_var() instead of calling this function.
     */
    int (*set_var) (yasm_arch *arch, const char *var, unsigned long val);

    /** Module-level implementation of yasm_arch_parse_cpu().
     * Call yasm_arch_parse_cpu() instead of calling this function.
     */
    void (*parse_cpu) (yasm_arch *arch, const char *cpuid,
		       unsigned long line);

    /** Module-level implementation of yasm_arch_parse_check_reg().
     * Call yasm_arch_parse_check_reg() instead of calling this function.
     */
    int (*parse_check_reg)
	(yasm_arch *arch, /*@out@*/ unsigned long data[1], const char *id,
	 unsigned long line);

    /** Module-level implementation of yasm_arch_parse_check_reggroup().
     * Call yasm_arch_parse_check_reggroup() instead of calling this function.
     */
    int (*parse_check_reggroup)
	(yasm_arch *arch, /*@out@*/ unsigned long data[1], const char *id,
	 unsigned long line);

    /** Module-level implementation of yasm_arch_parse_check_segreg().
     * Call yasm_arch_parse_check_segreg() instead of calling this function.
     */
    int (*parse_check_segreg)
	(yasm_arch *arch, /*@out@*/ unsigned long data[1], const char *id,
	 unsigned long line);

    /** Module-level implementation of yasm_arch_parse_check_insn().
     * Call yasm_arch_parse_check_insn() instead of calling this function.
     */
    int (*parse_check_insn)
	(yasm_arch *arch, /*@out@*/ unsigned long data[4], const char *id,
	 unsigned long line);

    /** Module-level implementation of yasm_arch_parse_check_prefix().
     * Call yasm_arch_parse_check_prefix() instead of calling this function.
     */
    int (*parse_check_prefix)
	(yasm_arch *arch, /*@out@*/ unsigned long data[4], const char *id,
	 unsigned long line);

    /** Module-level implementation of yasm_arch_parse_check_targetmod().
     * Call yasm_arch_parse_check_targetmod() instead of calling this function.
     */
    int (*parse_check_targetmod)
	(yasm_arch *arch, /*@out@*/ unsigned long data[1], const char *id,
	 unsigned long line);

    /** Module-level implementation of yasm_arch_parse_directive().
     * Call yasm_arch_parse_directive() instead of calling this function.
     */
    int (*parse_directive) (yasm_arch *arch, const char *name,
			    yasm_valparamhead *valparams,
			    /*@null@*/ yasm_valparamhead *objext_valparams,
			    yasm_object *object, unsigned long line);

    /** Module-level implementation of yasm_arch_get_fill().
     * Call yasm_arch_get_fill() instead of calling this function.
     */
    const unsigned char ** (*get_fill) (const yasm_arch *arch);

    /** Module-level implementation of yasm_arch_finalize_insn().
     * Call yasm_arch_finalize_insn() instead of calling this function.
     */
    void (*finalize_insn)
	(yasm_arch *arch, yasm_bytecode *bc, yasm_bytecode *prev_bc,
	 const unsigned long data[4], int num_operands,
	 /*@null@*/ yasm_insn_operands *operands, int num_prefixes,
	 unsigned long **prefixes, int num_segregs,
	 const unsigned long *segregs);

    /** Module-level implementation of yasm_arch_floatnum_tobytes().
     * Call yasm_arch_floatnum_tobytes() instead of calling this function.
     */
    int (*floatnum_tobytes) (yasm_arch *arch, const yasm_floatnum *flt,
			     unsigned char *buf, size_t destsize,
			     size_t valsize, size_t shift, int warn,
			     unsigned long line);

    /** Module-level implementation of yasm_arch_intnum_fixup_rel().
     * Call yasm_arch_intnum_fixup_rel() instead of calling this function.
     */
    int (*intnum_fixup_rel) (yasm_arch *arch, yasm_intnum *intn,
			     size_t valsize, const yasm_bytecode *bc,
			     unsigned long line);

    /** Module-level implementation of yasm_arch_intnum_tobytes().
     * Call yasm_arch_intnum_tobytes() instead of calling this function.
     */
    int (*intnum_tobytes) (yasm_arch *arch, const yasm_intnum *intn,
			   unsigned char *buf, size_t destsize, size_t valsize,
			   int shift, const yasm_bytecode *bc,
			   int warn, unsigned long line);

    /** Module-level implementation of yasm_arch_get_reg_size().
     * Call yasm_arch_get_reg_size() instead of calling this function.
     */
    unsigned int (*get_reg_size) (yasm_arch *arch, unsigned long reg);

    /** Module-level implementation of yasm_arch_reggroup_get_reg().
     * Call yasm_arch_reggroup_get_reg() instead of calling this function.
     */
    unsigned long (*reggroup_get_reg) (yasm_arch *arch, unsigned long reggroup,
				       unsigned long regindex);

    /** Module-level implementation of yasm_arch_reg_print().
     * Call yasm_arch_reg_print() instead of calling this function.
     */
    void (*reg_print) (yasm_arch *arch, unsigned long reg, FILE *f);

    /** Module-level implementation of yasm_arch_segreg_print().
     * Call yasm_arch_segreg_print() instead of calling this function.
     */
    void (*segreg_print) (yasm_arch *arch, unsigned long segreg, FILE *f);

    /** Module-level implementation of yasm_arch_ea_create().
     * Call yasm_arch_ea_create() instead of calling this function.
     */
    yasm_effaddr * (*ea_create) (yasm_arch *arch, /*@keep@*/ yasm_expr *e);

    /** NULL-terminated list of machines for this architecture.
     * Call yasm_arch_get_machine() to get the active machine of a particular
     * #yasm_arch.
     */
    yasm_arch_machine *machines;

    /** Default machine keyword.
     * Call yasm_arch_get_machine() to get the active machine of a particular
     * #yasm_arch.
     */
    const char *default_machine_keyword;

    /** Canonical "word" size in bytes.
     * Call yasm_arch_wordsize() to get the word size of a particular
     * #yasm_arch.
     */
    unsigned int wordsize;
} yasm_arch_module;

#ifdef YASM_LIB_INTERNAL
/** An instruction operand.  \internal */
struct yasm_insn_operand {
    /** Link for building linked list of operands.  \internal */
    /*@reldef@*/ STAILQ_ENTRY(yasm_insn_operand) link;

    /** Operand type. */
    enum {
	YASM_INSN__OPERAND_REG = 1,	/**< A register. */
	YASM_INSN__OPERAND_SEGREG,	/**< A segment register. */
	YASM_INSN__OPERAND_MEMORY,	/**< An effective address
					 *   (memory reference). */
	YASM_INSN__OPERAND_IMM		/**< An immediate or jump target. */
    } type;

    /** Operand data. */
    union {
	unsigned long reg;  /**< Arch data for reg/segreg. */
	yasm_effaddr *ea;   /**< Effective address for memory references. */
	yasm_expr *val;	    /**< Value of immediate or jump target. */
    } data;

    unsigned long targetmod;	/**< Arch target modifier, 0 if none. */

    /** Specified size of the operand, in bytes.  0 if not user-specified. */
    unsigned int size;

    /** Nonzero if dereference.  Used for "*foo" in GAS.
     * The reason for this is that by default in GAS, an unprefixed value
     * is a memory address, except for jumps/calls, in which case it needs a
     * "*" prefix to become a memory address (otherwise it's an immediate).
     * This isn't knowable in the parser stage, so the parser sets this flag
     * to indicate the "*" prefix has been used, and the arch needs to adjust
     * the operand type appropriately depending on the instruction type.
     */
    int deref;
};
#endif

/** Get the one-line description of an architecture.
 * \param arch	    architecture
 * \return One-line description of architecture.
 */
const char *yasm_arch_name(const yasm_arch *arch);

/** Get the keyword used to select an architecture.
 * \param arch	    architecture
 * \return Architecture keyword.
 */
const char *yasm_arch_keyword(const yasm_arch *arch);

/** Get the word size of an architecture.
 * \param arch	    architecture
 * \return Word size (in bits).
 */
unsigned int yasm_arch_wordsize(const yasm_arch *arch);

/** Create architecture.
 * \param module	architecture module
 * \param machine	keyword of machine in use (must be one listed in
 *			#machines)
 * \param parser	keyword of parser in use
 * \param error		error return value
 * \return NULL on error (error returned in error parameter), otherwise new
 *	   architecture.
 */
/*@only@*/ yasm_arch *yasm_arch_create(const yasm_arch_module *module,
				       const char *machine, const char *parser,
				       /*@out@*/ yasm_arch_create_error *error);

/** Clean up, free any architecture-allocated memory.
 * \param arch	architecture
 */
void yasm_arch_destroy(/*@only@*/ yasm_arch *arch);

/** Get architecture's active machine name.
 * \param arch	architecture
 * \return Active machine name.
 */
const char *yasm_arch_get_machine(const yasm_arch *arch);

/** Set any arch-specific variables.  For example, "mode_bits" in x86.
 * \param arch	architecture
 * \param var	variable name
 * \param val	value to set
 * \return Zero on success, non-zero on failure (variable does not exist).
 */
int yasm_arch_set_var(yasm_arch *arch, const char *var, unsigned long val);

/** Switch available instructions/registers/etc based on a user-specified
 * CPU identifier.  Should modify behavior ONLY of parse_* functions!  The
 * bytecode and output functions should be able to handle any CPU.
 * \param arch	architecture
 * \param cpuid	cpu identifier as in the input file
 * \param line	virtual line (from yasm_linemap)
 */
void yasm_arch_parse_cpu(yasm_arch *arch, const char *cpuid,
			 unsigned long line);

/** Check an generic identifier to see if it matches architecture specific
 * names for registers.  Unrecognized identifiers should return 0
 * so they can be treated as normal symbols.  Any additional data beyond
 * just the type (almost always necessary) should be returned into the
 * space provided by the data parameter.
 * \param arch	architecture
 * \param data	extra identification information (yasm_arch-specific)
 *		[output]
 * \param id	identifier as in the input file
 * \param line	virtual line (from yasm_linemap)
 * \return 1 if id is a register, 0 if not.
 */
int yasm_arch_parse_check_reg
    (yasm_arch *arch, /*@out@*/ unsigned long data[1], const char *id,
     unsigned long line);

/** Check an generic identifier to see if it matches architecture specific
 * names for register groups.  Unrecognized identifiers should return 0
 * so they can be treated as normal symbols.  Any additional data beyond
 * just the type (almost always necessary) should be returned into the
 * space provided by the data parameter.
 * \param arch	architecture
 * \param data	extra identification information (yasm_arch-specific)
 *		[output]
 * \param id	identifier as in the input file
 * \param line	virtual line (from yasm_linemap)
 * \return 1 if id is a register, 0 if not.
 */
int yasm_arch_parse_check_reggroup
    (yasm_arch *arch, /*@out@*/ unsigned long data[1], const char *id,
     unsigned long line);

/** Check an generic identifier to see if it matches architecture specific
 * names for segment registers.  Unrecognized identifiers should return 0
 * so they can be treated as normal symbols.  Any additional data beyond
 * just the type (almost always necessary) should be returned into the
 * space provided by the data parameter.
 * \param arch	architecture
 * \param data	extra identification information (yasm_arch-specific)
 *		[output]
 * \param id	identifier as in the input file
 * \param line	virtual line (from yasm_linemap)
 * \return 1 if id is a register, 0 if not.
 */
int yasm_arch_parse_check_segreg
    (yasm_arch *arch, /*@out@*/ unsigned long data[1], const char *id,
     unsigned long line);

/** Check an generic identifier to see if it matches architecture specific
 * names for instructions.  Unrecognized identifiers should return 0
 * so they can be treated as normal symbols.  Any additional data beyond
 * just the type (almost always necessary) should be returned into the
 * space provided by the data parameter.
 * \param arch	architecture
 * \param data	extra identification information (yasm_arch-specific)
 *		[output]
 * \param id	identifier as in the input file
 * \param line	virtual line (from yasm_linemap)
 * \return 1 if id is a register, 0 if not.
 */
int yasm_arch_parse_check_insn
    (yasm_arch *arch, /*@out@*/ unsigned long data[4], const char *id,
     unsigned long line);

/** Check an generic identifier to see if it matches architecture specific
 * names for prefixes.  Unrecognized identifiers should return 0
 * so they can be treated as normal symbols.  Any additional data beyond
 * just the type (almost always necessary) should be returned into the
 * space provided by the data parameter.
 * \param arch	architecture
 * \param data	extra identification information (yasm_arch-specific)
 *		[output]
 * \param id	identifier as in the input file
 * \param line	virtual line (from yasm_linemap)
 * \return 1 if id is a register, 0 if not.
 */
int yasm_arch_parse_check_prefix
    (yasm_arch *arch, /*@out@*/ unsigned long data[4], const char *id,
     unsigned long line);

/** Check an generic identifier to see if it matches architecture specific
 * names for target modifiers.  Unrecognized identifiers should return 0
 * so they can be treated as normal symbols.  Any additional data beyond
 * just the type (almost always necessary) should be returned into the
 * space provided by the data parameter.
 * \param arch	architecture
 * \param data	extra identification information (yasm_arch-specific)
 *		[output]
 * \param id	identifier as in the input file
 * \param line	virtual line (from yasm_linemap)
 * \return 1 if id is a register, 0 if not.
 */
int yasm_arch_parse_check_targetmod
    (yasm_arch *arch, /*@out@*/ unsigned long data[1], const char *id,
     unsigned long line);

/** Handle architecture-specific directives.
 * Should modify behavior ONLY of parse functions, much like parse_cpu().
 * \param arch			architecture
 * \param name			directive name
 * \param valparams		value/parameters
 * \param objext_valparams	object format extensions
 *				value/parameters
 * \param object		object
 * \param line			virtual line (as from yasm_linemap)
 * \return Nonzero if directive was not recognized; 0 if directive was
 *	   recognized, even if it wasn't valid.
 */
int yasm_arch_parse_directive(yasm_arch *arch, const char *name,
			      yasm_valparamhead *valparams,
			      /*@null@*/ yasm_valparamhead *objext_valparams,
			      yasm_object *object, unsigned long line);

/** Get NOP fill patterns for 1-15 bytes of fill.
 * \param arch		architecture
 * \return 16-entry array of arrays; [0] is unused, [1] - [15] point to arrays
 * of 1-15 bytes (respectively) in length.
 */
const unsigned char **yasm_arch_get_fill(const yasm_arch *arch);

/** Finalize an instruction from a semi-generic insn description.  Note an
 * existing bytecode is required.
 * \param arch		architecture
 * \param bc		bytecode to finalize
 * \param prev_bc	previous bytecode in section
 * \param data		instruction data (from parse_check_id()); all
 *				zero indicates an empty instruction
 * \param num_operands	number of operands
 * \param operands	list of operands (in parse order)
 * \param num_prefixes	number of prefixes
 * \param prefixes	array of 4-element prefix data
 * \param num_segregs	number of segment register prefixes
 * \param segregs	array of segment register data
 * \return If no match is found (the instruction is invalid), no action is
 *         performed and an error is recorded.
 */
void yasm_arch_finalize_insn
    (yasm_arch *arch, yasm_bytecode *bc, yasm_bytecode *prev_bc,
     const unsigned long data[4], int num_operands,
     /*@null@*/ yasm_insn_operands *operands, int num_prefixes,
     const unsigned long **prefixes, int num_segregs,
     const unsigned long *segregs);

/** Output #yasm_floatnum to buffer.  Puts the value into the least
 * significant bits of the destination, or may be shifted into more
 * significant bits by the shift parameter.  The destination bits are
 * cleared before being set.
 * Architecture-specific because of endianness.
 * \param arch		architecture
 * \param flt		floating point value
 * \param buf		buffer to write into
 * \param destsize	destination size (in bytes)
 * \param valsize	size (in bits)
 * \param shift		left shift (in bits)
 * \param warn		enables standard overflow/underflow warnings
 * \param line		virtual line; may be 0 if warn is 0.
 * \return Nonzero on error.
 */
int yasm_arch_floatnum_tobytes(yasm_arch *arch, const yasm_floatnum *flt,
			       unsigned char *buf, size_t destsize,
			       size_t valsize, size_t shift, int warn,
			       unsigned long line);

/** Adjust #yasm_intnum for relative displacement from bc.  Displacement
 * is modified in-place.
 * \param arch		architecture
 * \param intn		integer value
 * \param valsize	size (in bits)
 * \param bc		bytecode being output ("parent" of value)
 * \param line		virtual line; may be 0 if warn is 0
 * \return Nonzero on error.
 */
int yasm_arch_intnum_fixup_rel(yasm_arch *arch, yasm_intnum *intn,
			       size_t valsize, const yasm_bytecode *bc,
			       unsigned long line);

/** Output #yasm_intnum to buffer.  Puts the value into the least
 * significant bits of the destination, or may be shifted into more
 * significant bits by the shift parameter.  The destination bits are
 * cleared before being set.
 * \param arch		architecture
 * \param intn		integer value
 * \param buf		buffer to write into
 * \param destsize	destination size (in bytes)
 * \param valsize	size (in bits)
 * \param shift		left shift (in bits); may be negative to specify right
 *			shift (standard warnings include truncation to boundary)
 * \param bc		bytecode being output ("parent" of value)
 * \param warn		enables standard warnings (value doesn't fit into
 *			valsize bits)
 * \param line		virtual line; may be 0 if warn is 0
 * \return Nonzero on error.
 */
int yasm_arch_intnum_tobytes(yasm_arch *arch, const yasm_intnum *intn,
			     unsigned char *buf, size_t destsize,
			     size_t valsize, int shift,
			     const yasm_bytecode *bc, int warn,
			     unsigned long line);

/** Get the equivalent byte size of a register.
 * \param arch	architecture
 * \param reg	register
 * \return 0 if there is no suitable equivalent size, otherwise the size.
 */
unsigned int yasm_arch_get_reg_size(yasm_arch *arch, unsigned long reg);

/** Get a specific register of a register group, based on the register
 * group and the index within the group.
 * \param arch		architecture
 * \param reggroup	register group
 * \param regindex	register index
 * \return 0 if regindex is not valid for that register group, otherwise the
 *         specific register value.
 */
unsigned long yasm_arch_reggroup_get_reg(yasm_arch *arch,
					 unsigned long reggroup,
					 unsigned long regindex);

/** Print a register.  For debugging purposes.
 * \param arch		architecture
 * \param reg		register
 * \param f		file
 */
void yasm_arch_reg_print(yasm_arch *arch, unsigned long reg, FILE *f);

/** Print a segment register.  For debugging purposes.
 * \param arch		architecture
 * \param segreg	segment register
 * \param f		file
 */
void yasm_arch_segreg_print(yasm_arch *arch, unsigned long segreg, FILE *f);

/** Create an effective address from an expression.
 * \param arch	architecture
 * \param e	expression (kept, do not delete)
 * \return Newly allocated effective address.
 */
yasm_effaddr *yasm_arch_ea_create(yasm_arch *arch, /*@keep@*/ yasm_expr *e);


#ifndef YASM_DOXYGEN

/* Inline macro implementations for arch functions */

#define yasm_arch_name(arch) \
    (((yasm_arch_base *)arch)->module->name)
#define yasm_arch_keyword(arch) \
    (((yasm_arch_base *)arch)->module->keyword)
#define yasm_arch_wordsize(arch) \
    (((yasm_arch_base *)arch)->module->wordsize)

#define yasm_arch_create(module, machine, parser, error) \
    module->create(machine, parser, error)

#define yasm_arch_destroy(arch) \
    ((yasm_arch_base *)arch)->module->destroy(arch)
#define yasm_arch_get_machine(arch) \
    ((yasm_arch_base *)arch)->module->get_machine(arch)
#define yasm_arch_set_var(arch, var, val) \
    ((yasm_arch_base *)arch)->module->set_var(arch, var, val)
#define yasm_arch_parse_cpu(arch, cpuid, line) \
    ((yasm_arch_base *)arch)->module->parse_cpu(arch, cpuid, line)
#define yasm_arch_parse_check_reg(arch, data, id, line) \
    ((yasm_arch_base *)arch)->module->parse_check_reg(arch, data, id, line)
#define yasm_arch_parse_check_reggroup(arch, data, id, line) \
    ((yasm_arch_base *)arch)->module->parse_check_reggroup(arch, data, id, \
							   line)
#define yasm_arch_parse_check_segreg(arch, data, id, line) \
    ((yasm_arch_base *)arch)->module->parse_check_segreg(arch, data, id, line)
#define yasm_arch_parse_check_insn(arch, data, id, line) \
    ((yasm_arch_base *)arch)->module->parse_check_insn(arch, data, id, line)
#define yasm_arch_parse_check_prefix(arch, data, id, line) \
    ((yasm_arch_base *)arch)->module->parse_check_prefix(arch, data, id, line)
#define yasm_arch_parse_check_targetmod(arch, data, id, line) \
    ((yasm_arch_base *)arch)->module->parse_check_targetmod(arch, data, id, \
							    line)
#define yasm_arch_parse_directive(arch, name, valparams, objext_valparams, \
				  object, line) \
    ((yasm_arch_base *)arch)->module->parse_directive \
	(arch, name, valparams, objext_valparams, object, line)
#define yasm_arch_get_fill(arch) \
    ((yasm_arch_base *)arch)->module->get_fill(arch)
#define yasm_arch_finalize_insn(arch, bc, prev_bc, data, num_operands, \
				operands, num_prefixes, prefixes, \
				num_segregs, segregs) \
    ((yasm_arch_base *)arch)->module->finalize_insn \
	(arch, bc, prev_bc, data, num_operands, operands, num_prefixes, \
	 prefixes, num_segregs, segregs)
#define yasm_arch_floatnum_tobytes(arch, flt, buf, destsize, valsize, shift, \
				   warn, line) \
    ((yasm_arch_base *)arch)->module->floatnum_tobytes \
	(arch, flt, buf, destsize, valsize, shift, warn, line)
#define yasm_arch_intnum_fixup_rel(arch, intn, valsize, bc, line) \
    ((yasm_arch_base *)arch)->module->intnum_fixup_rel \
	(arch, intn, valsize, bc, line)
#define yasm_arch_intnum_tobytes(arch, intn, buf, destsize, valsize, shift, \
				 bc, warn, line) \
    ((yasm_arch_base *)arch)->module->intnum_tobytes \
	(arch, intn, buf, destsize, valsize, shift, bc, warn, line)
#define yasm_arch_get_reg_size(arch, reg) \
    ((yasm_arch_base *)arch)->module->get_reg_size(arch, reg)
#define yasm_arch_reggroup_get_reg(arch, regg, regi) \
    ((yasm_arch_base *)arch)->module->reggroup_get_reg(arch, regg, regi)
#define yasm_arch_reg_print(arch, reg, f) \
    ((yasm_arch_base *)arch)->module->reg_print(arch, reg, f)
#define yasm_arch_segreg_print(arch, segreg, f) \
    ((yasm_arch_base *)arch)->module->segreg_print(arch, segreg, f)
#define yasm_arch_ea_create(arch, e) \
    ((yasm_arch_base *)arch)->module->ea_create(arch, e)

#endif

/** Create an instruction operand from a register.
 * \param reg	register
 * \return Newly allocated operand.
 */
yasm_insn_operand *yasm_operand_create_reg(unsigned long reg);

/** Create an instruction operand from a segment register.
 * \param segreg	segment register
 * \return Newly allocated operand.
 */
yasm_insn_operand *yasm_operand_create_segreg(unsigned long segreg);

/** Create an instruction operand from an effective address.
 * \param ea	effective address
 * \return Newly allocated operand.
 */
yasm_insn_operand *yasm_operand_create_mem(/*@only@*/ yasm_effaddr *ea);

/** Create an instruction operand from an immediate expression.
 * Looks for cases of a single register and creates a register variant of
 * #yasm_insn_operand.
 * \param val	immediate expression
 * \return Newly allocated operand.
 */
yasm_insn_operand *yasm_operand_create_imm(/*@only@*/ yasm_expr *val);

/** Print an instruction operand.  For debugging purposes.
 * \param arch		architecture
 * \param f		file
 * \param indent_level	indentation level
 * \param op		instruction operand
 */
void yasm_operand_print(const yasm_insn_operand *op, FILE *f, int indent_level,
			yasm_arch *arch);

/** Create a new list of instruction operands.
 * \return Newly allocated list.
 */
yasm_insn_operands *yasm_ops_create(void);

/** Destroy a list of instruction operands (created with yasm_ops_create()).
 * \param headp		list of instruction operands
 * \param content	if nonzero, deletes content of each operand
 */
void yasm_ops_destroy(yasm_insn_operands *headp, int content);

/** Get the first operand in a list of instruction operands.
 * \param headp		list of instruction operands
 * \return First operand in list (NULL if list is empty).
 */
yasm_insn_operand *yasm_ops_first(yasm_insn_operands *headp);

/** Get the next operand in a list of instruction operands.
 * \param cur		previous operand
 * \return Next operand in list (NULL if cur was the last operand).
 */
yasm_insn_operand *yasm_operand_next(yasm_insn_operand *cur);

#ifdef YASM_LIB_INTERNAL
#define yasm_ops_initialize(headp)	STAILQ_INIT(headp)
#define yasm_ops_first(headp)		STAILQ_FIRST(headp)
#define yasm_operand_next(cur)		STAILQ_NEXT(cur, link)

/** Delete (free allocated memory for) a list of instruction operands (created
 * with yasm_ops_initialize()).
 * \param headp		list of instruction operands
 * \param content	if nonzero, deletes content of each operand
 */
void yasm_ops_delete(yasm_insn_operands *headp, int content);
#endif

/** Add data value to the end of a list of instruction operands.
 * \note Does not make a copy of the operand; so don't pass this function
 *	 static or local variables, and discard the op pointer after calling
 *	 this function.
 * \param headp		list of instruction operands
 * \param op		operand (may be NULL)
 * \return If operand was actually appended (it wasn't NULL), the operand;
 *	   otherwise NULL.
 */
/*@null@*/ yasm_insn_operand *yasm_ops_append
    (yasm_insn_operands *headp,
     /*@returned@*/ /*@null@*/ yasm_insn_operand *op);

/** Print a list of instruction operands.  For debugging purposes.
 * \param arch		architecture
 * \param f		file
 * \param indent_level	indentation level
 * \param headp		list of instruction operands
 */
void yasm_ops_print(const yasm_insn_operands *headp, FILE *f, int indent_level,
		    yasm_arch *arch);

#endif
