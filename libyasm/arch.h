/**
 * \file libyasm/arch.h
 * \brief YASM architecture interface.
 *
 * \rcs
 * $IdPath$
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

/** Return value from yasm_arch::parse_check_id(). */
typedef enum {
    YASM_ARCH_CHECK_ID_NONE = 0,	/**< Just a normal identifier. */
    YASM_ARCH_CHECK_ID_INSN,		/**< An instruction. */
    YASM_ARCH_CHECK_ID_PREFIX,		/**< An instruction prefix. */
    YASM_ARCH_CHECK_ID_REG,		/**< A register. */
    YASM_ARCH_CHECK_ID_SEGREG,/**< a segment register (for memory overrides). */
    YASM_ARCH_CHECK_ID_TARGETMOD	/**< A target modifier (for jumps) */
} yasm_arch_check_id_retval;

/** An instruction operand (opaque type). */
typedef struct yasm_insn_operand yasm_insn_operand;
/** A list of instruction operands (opaque type).
 * The list goes from left-to-right as parsed.
 */
typedef struct yasm_insn_operandhead yasm_insn_operandhead;
#ifdef YASM_LIB_INTERNAL
/*@reldef@*/ STAILQ_HEAD(yasm_insn_operandhead, yasm_insn_operand);
#endif

/** "Flavor" of the parser.
 * Different assemblers order instruction operands differently.  Also, some
 * differ on how exactly various registers are specified.  There's no great
 * solution to this, as the parsers aren't supposed to have knowledge of the
 * architectural internals, and the architecture is supposed to be parser
 * independent.  To make things work, as a rather hackish solution, we give the
 * architecture a little knowledge about the general "flavor" of the parser,
 * and let the architecture decide what to do with it.  Most architectures will
 * probably not even use this, but it's required for some (x86 in particular)
 * for correct behavior on all parsers.
 */
typedef enum {
    YASM_ARCH_SYNTAX_FLAVOR_NASM = 1,	/**< Like NASM */
    YASM_ARCH_SYNTAX_FLAVOR_GAS		/**< Like GAS */
} yasm_arch_syntax_flavor;

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

/** YASM architecture interface.
 * \note All "data" in parser-related functions (parse_*) needs to start the
 *	 parse initialized to 0 to make it okay for a parser-related function
 *	 to use/check previously stored data to see if it's been called before
 *	 on the same piece of data.
 */
struct yasm_arch {
    /** One-line description of the architecture. */
    const char *name;

    /** Keyword used to select architecture. */
    const char *keyword;

    /** Initialize architecture for use.
     * \param machine	keyword of machine in use (must be one listed in
     *			#machines)
     * \return Nonzero if machine not recognized.
     */
    int (*initialize) (const char *machine);

    /** Clean up, free any architecture-allocated memory. */
    void (*cleanup) (void);

    /** Switch available instructions/registers/etc based on a user-specified
     * CPU identifier.  Should modify behavior ONLY of parse_* functions!  The
     * bytecode and output functions should be able to handle any CPU.
     * \param cpuid	cpu identifier as in the input file
     * \param lindex	line index (as from yasm_linemgr)
     */
    void (*parse_cpu) (const char *cpuid, unsigned long lindex);

    /** Check an generic identifier to see if it matches architecture specific
     * names for instructions, registers, etc.  Unrecognized identifiers should
     * be returned as #YASM_ARCH_CHECK_ID_NONE so they can be treated as normal
     * symbols.  Any additional data beyond just the type (almost always
     * necessary) should be returned into the space provided by the data
     * parameter.
     * \note Even though this is passed a data[4], only data[0] should be used
     *	     for #YASM_ARCH_CHECK_ID_TARGETMOD, #YASM_ARCH_CHECK_ID_REG, and
     *	     #YASM_ARCH_CHECK_ID_SEGREG return values.
     * \param data	extra identification information (yasm_arch-specific)
     *			[output]
     * \param id	identifier as in the input file
     * \param lindex	line index (as from yasm_linemgr)
     * \return General type of identifier (#yasm_arch_check_id_retval).
     */
    yasm_arch_check_id_retval (*parse_check_id)
	(unsigned long data[4], const char *id, unsigned long lindex);

    /** Handle architecture-specific directives.
     * Should modify behavior ONLY of parse functions, much like parse_cpu().
     * \param name		directive name
     * \param valparams		value/parameters
     * \param objext_valparams	object format extensions
     *				value/parameters
     * \param headp		list of sections
     * \param lindex		line index (as from yasm_linemgr)
     * \return Nonzero if directive was not recognized; 0 if directive was
     *	       recognized, even if it wasn't valid.
     */
    int (*parse_directive) (const char *name, yasm_valparamhead *valparams,
			    /*@null@*/ yasm_valparamhead *objext_valparams,
			    yasm_sectionhead *headp, unsigned long lindex);

    /** Create an instruction.  Creates a bytecode by matching the
     * instruction data and the parameters given with a valid instruction.
     * \param data		instruction data (from parse_check_id()); all
     *				zero indicates an empty instruction
     * \param num_operands	number of operands parsed
     * \param operands		list of operands (in parse order)
     * \param cur_section	currently active section
     * \param prev_bc		previously parsed bytecode in section (NULL if
     *				first bytecode in section)
     * \param lindex		line index (as from yasm_linemgr)
     * \return If no match is found (the instruction is invalid), NULL,
     *	       otherwise newly allocated bytecode containing instruction.
     */
    /*@null@*/ yasm_bytecode * (*parse_insn)
	(const unsigned long data[4], int num_operands,
	 /*@null@*/ yasm_insn_operandhead *operands, yasm_section *cur_section,
	 /*@null@*/ yasm_bytecode *prev_bc, unsigned long lindex);

    /** Handle an instruction prefix.
     * Modifies an instruction bytecode based on the prefix in data.
     * \param bc	bytecode (must be instruction bytecode)
     * \param data	prefix (from parse_check_id())
     * \param lindex	line index (as from yasm_linemgr)
     */
    void (*parse_prefix) (yasm_bytecode *bc, const unsigned long data[4],
			  unsigned long lindex);

    /** Handle an segment register instruction prefix.
     * Modifies an instruction bytecode based on a segment register prefix.
     * \param bc	bytecode (must be instruction bytecode)
     * \param segreg	segment register (from parse_check_id())
     * \param lindex	line index (as from yasm_linemgr)
     */
    void (*parse_seg_prefix) (yasm_bytecode *bc, unsigned long segreg,
			      unsigned long lindex);

    /** Handle a memory expression segment override.
     * Modifies an instruction bytecode based on a segment override in a
     * memory expression.
     * \param bc	bytecode (must be instruction bytecode)
     * \param segreg	segment register (from parse_check_id())
     * \param lindex	line index (as from yasm_linemgr)
     */
    void (*parse_seg_override) (yasm_effaddr *ea, unsigned long segreg,
				unsigned long lindex);

    /** Maximum used bytecode type value+1.  Should be set to
     * #YASM_BYTECODE_TYPE_BASE if no additional bytecode types are defined
     * by the architecture.
     * \internal
     */
    const int bc_type_max;

    /** Delete a yasm_arch-defined bytecode.
     * \internal Do not call directly, instead call yasm_bc_delete().
     *
     * \copydoc yasm_bc_delete()
     */
    void (*bc_delete) (yasm_bytecode *bc);

    /** Print a yasm_arch-defined bytecode.
     * \internal Do not call directly, instead call yasm_bc_print().
     *
     * \copydoc yasm_bc_print()
     */
    void (*bc_print) (FILE *f, int indent_level, const yasm_bytecode *bc);

    /** Resolve labels in a yasm_arch-defined bytecode.
     * \internal Do not call directly, instead call yasm_bc_resolve().
     *
     * \copydoc yasm_bc_resolve()
     */
    yasm_bc_resolve_flags (*bc_resolve)
	(yasm_bytecode *bc, int save, const yasm_section *sect,
	 yasm_calc_bc_dist_func calc_bc_dist);

    /** Convert a yasm_arch-defined bytecode into its byte representation.
     * \internal Do not call directly, instead call yasm_bc_tobytes().
     *
     * \copydoc yasm_bc_tobytes()
     */
    int (*bc_tobytes) (yasm_bytecode *bc, unsigned char **bufp,
		       const yasm_section *sect, void *d,
		       yasm_output_expr_func output_expr);

    /** Output #yasm_floatnum to buffer.  Puts the value into the least
     * significant bits of the destination, or may be shifted into more
     * significant bits by the shift parameter.  The destination bits are
     * cleared before being set.
     * Architecture-specific because of endianness.
     * \param flt	floating point value
     * \param buf	buffer to write into
     * \param destsize	destination size (in bytes)
     * \param valsize	size (in bits)
     * \param shift	left shift (in bits)
     * \param warn	enables standard overflow/underflow warnings
     * \param lindex	line index; may be 0 if warn is 0.
     * \return Nonzero on error.
     */
    int (*floatnum_tobytes) (const yasm_floatnum *flt, unsigned char *buf,
			     size_t destsize, size_t valsize, size_t shift,
			     int warn, unsigned long lindex);

    /** Output #yasm_intnum to buffer.  Puts the value into the least
     * significant bits of the destination, or may be shifted into more
     * significant bits by the shift parameter.  The destination bits are
     * cleared before being set.
     * \param intn	integer value
     * \param buf	buffer to write into
     * \param destsize	destination size (in bytes)
     * \param valsize	size (in bits)
     * \param shift	left shift (in bits); may be negative to specify right
     *			shift (standard warnings include truncation to boundary)
     * \param bc	bytecode being output ("parent" of value)
     * \param rel	value is a relative displacement from bc
     * \param warn	enables standard warnings (value doesn't fit into
     *			valsize bits)
     * \param lindex	line index; may be 0 if warn is 0
     * \return Nonzero on error.
     */
    int (*intnum_tobytes) (const yasm_intnum *intn, unsigned char *buf,
			   size_t destsize, size_t valsize, int shift,
			   const yasm_bytecode *bc, int rel, int warn,
			   unsigned long lindex);

    /** Get the equivalent byte size of a register.
     * \param reg	register
     * \return 0 if there is no suitable equivalent size, otherwise the size.
     */
    unsigned int (*get_reg_size) (unsigned long reg);

    /** Print a register.  For debugging purposes.
     * \param f			file
     * \param indent_level	indentation level
     * \param reg		register
     */
    void (*reg_print) (FILE *f, unsigned long reg);

    /** Print a segment register.  For debugging purposes.
     * \param f			file
     * \param indent_level	indentation level
     * \param segreg		segment register
     */
    void (*segreg_print) (FILE *f, unsigned long segreg);

    /** Create an effective address from an expression.
     * \param e	expression (kept, do not delete)
     * \return Newly allocated effective address.
     */
    yasm_effaddr * (*ea_new_expr) (/*@keep@*/ yasm_expr *e);

    /** Delete the yasm_arch-specific data in an effective address.
     * May be NULL if no special deletion is required (e.g. there's no
     * dynamically allocated pointers in the effective address data).
     * \internal Do not call directly, instead call yasm_ea_delete().
     *
     * \copydoc yasm_ea_delete()
     */
    void (*ea_data_delete) (yasm_effaddr *ea);

    /** Print the yasm_arch-specific data in an effective address.
     * \internal Do not call directly, instead call yasm_ea_print().
     *
     * \copydoc yasm_ea_print()
     */
    void (*ea_data_print) (FILE *f, int indent_level, const yasm_effaddr *ea);

    /** NULL-terminated list of machines for this architecture. */
    yasm_arch_machine *machines;

    /** Default machine keyword. */
    const char *default_machine_keyword;
};

#ifdef YASM_LIB_INTERNAL
/** An instruction operand. */
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
};
#endif

/** Common initializer for yasm_arch helper functions.
 * \param a	architecture in use
 */
void yasm_arch_common_initialize(yasm_arch *a);

/** Create an instruction operand from a register.
 * \param reg	register
 * \return Newly allocated operand.
 */
yasm_insn_operand *yasm_operand_new_reg(unsigned long reg);

/** Create an instruction operand from a segment register.
 * \param segreg	segment register
 * \return Newly allocated operand.
 */
yasm_insn_operand *yasm_operand_new_segreg(unsigned long segreg);

/** Create an instruction operand from an effective address.
 * \param ea	effective address
 * \return Newly allocated operand.
 */
yasm_insn_operand *yasm_operand_new_mem(/*@only@*/ yasm_effaddr *ea);

/** Create an instruction operand from an immediate expression.
 * Looks for cases of a single register and creates a register variant of
 * #yasm_insn_operand.
 * \param val	immediate expression
 * \return Newly allocated operand.
 */
yasm_insn_operand *yasm_operand_new_imm(/*@only@*/ yasm_expr *val);

/** Print an instruction operand.  For debugging purposes.
 * \param f		file
 * \param indent_level	indentation level
 * \param op		instruction operand
 */
void yasm_operand_print(FILE *f, int indent_level,
			const yasm_insn_operand *op);

/** Initialize a list of instruction operands.
 * \param headp	list of instruction operands
 */
void yasm_ops_initialize(yasm_insn_operandhead *headp);

/** Get the first operand in a list of instruction operands.
 * \param headp		list of instruction operands
 * \return First operand in list (NULL if list is empty).
 */
yasm_insn_operand *yasm_ops_first(yasm_insn_operandhead *headp);

/** Get the next operand in a list of instruction operands.
 * \param cur		previous operand
 * \return Next operand in list (NULL if cur was the last operand).
 */
yasm_insn_operand *yasm_ops_next(yasm_insn_operand *cur);

#ifdef YASM_LIB_INTERNAL
#define yasm_ops_initialize(headp)	STAILQ_INIT(headp)
#define yasm_ops_first(headp)		STAILQ_FIRST(headp)
#define yasm_ops_next(cur)		STAILQ_NEXT(cur, link)
#endif

/** Delete (free allocated memory for) a list of instruction operands.
 * \param headp		list of instruction operands
 * \param content	if nonzero, deletes content of each operand
 */
void yasm_ops_delete(yasm_insn_operandhead *headp, int content);

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
    (yasm_insn_operandhead *headp,
     /*@returned@*/ /*@null@*/ yasm_insn_operand *op);

/** Print a list of instruction operands.  For debugging purposes.
 * \param f		file
 * \param indent_level	indentation level
 * \param headp		list of instruction operands
 */
void yasm_ops_print(FILE *f, int indent_level,
		    const yasm_insn_operandhead *headp);

#endif
