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

/** Return value from yasm_arch_module::parse_check_id(). */
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
typedef struct yasm_insn_operands yasm_insn_operands;
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

/** Version number of #yasm_arch_module interface.  Any functional change to
 * the #yasm_arch_module interface should simultaneously increment this number.
 * This version should be checked by #yasm_arch_module loaders (using
 * yasm_arch_module_get_version()) to verify that the expected version (the
 * version defined by its libyasm header files) matches the loaded module
 * version (the version defined by the module's libyasm header files).  Doing
 * this will ensure that the module version's function definitions match the
 * module loader's function definitions.  The version number must never be
 * decreased.
 */
#define YASM_ARCH_VERSION	2

/** YASM architecture module interface.
 * \note All "data" in parser-related functions (parse_*) needs to start the
 *	 parse initialized to 0 to make it okay for a parser-related function
 *	 to use/check previously stored data to see if it's been called before
 *	 on the same piece of data.
 */
typedef struct yasm_arch_module yasm_arch_module;

#ifndef YASM_DOXYGEN
struct yasm_arch_module {
    /** Version (see #YASM_ARCH_VERSION).  Should always be set to
     * #YASM_ARCH_VERSION by the module source and checked against
     * #YASM_ARCH_VERSION by the module loader.
     */
    unsigned int version;

    /** One-line description of the architecture. */
    const char *name;

    /** Keyword used to select architecture. */
    const char *keyword;

    /** Initialize architecture for use.
     * \param machine	keyword of machine in use (must be one listed in
     *			#machines)
     * \return NULL if machine not recognized.
     */
    /*@only@*/ yasm_arch * (*create) (const char *machine);

    /** \copydoc yasm_arch_destroy() */
    void (*destroy) (/*@only@*/ yasm_arch *arch);

    /** \copydoc yasm_arch_get_machine() */
    const char * (*get_machine) (const yasm_arch *arch);

    /** \copydoc yasm_arch_set_var() */
    int (*set_var) (yasm_arch *arch, const char *var, unsigned long val);

    /** \copydoc yasm_arch_parse_cpu() */
    void (*parse_cpu) (yasm_arch *arch, const char *cpuid,
		       unsigned long line);

    /** \copydoc yasm_arch_parse_check_id() */
    yasm_arch_check_id_retval (*parse_check_id)
	(yasm_arch *arch, unsigned long data[4], const char *id,
	 unsigned long line);

    /** \copydoc yasm_arch_parse_directive() */
    int (*parse_directive) (yasm_arch *arch, const char *name,
			    yasm_valparamhead *valparams,
			    /*@null@*/ yasm_valparamhead *objext_valparams,
			    yasm_object *object, unsigned long line);

    /** \copydoc yasm_arch_parse_insn() */
    /*@null@*/ yasm_bytecode * (*parse_insn)
	(yasm_arch *arch, const unsigned long data[4], int num_operands,
	 /*@null@*/ yasm_insn_operands *operands, yasm_bytecode *prev_bc,
	 unsigned long line);

    /** \copydoc yasm_arch_parse_prefix() */
    void (*parse_prefix) (yasm_arch *arch, yasm_bytecode *bc,
			  const unsigned long data[4], unsigned long line);

    /** \copydoc yasm_arch_parse_seg_prefix() */
    void (*parse_seg_prefix) (yasm_arch *arch, yasm_bytecode *bc,
			      unsigned long segreg, unsigned long line);

    /** \copydoc yasm_arch_parse_seg_override() */
    void (*parse_seg_override) (yasm_arch *arch, yasm_effaddr *ea,
				unsigned long segreg, unsigned long line);

    /** \copydoc yasm_arch_floatnum_tobytes() */
    int (*floatnum_tobytes) (yasm_arch *arch, const yasm_floatnum *flt,
			     unsigned char *buf, size_t destsize,
			     size_t valsize, size_t shift, int warn,
			     unsigned long line);

    /** \copydoc yasm_arch_intnum_tobytes() */
    int (*intnum_tobytes) (yasm_arch *arch, const yasm_intnum *intn,
			   unsigned char *buf, size_t destsize, size_t valsize,
			   int shift, const yasm_bytecode *bc, int rel,
			   int warn, unsigned long line);

    /** \copydoc yasm_arch_get_reg_size() */
    unsigned int (*get_reg_size) (yasm_arch *arch, unsigned long reg);

    /** \copydoc yasm_arch_reg_print() */
    void (*reg_print) (yasm_arch *arch, unsigned long reg, FILE *f);

    /** \copydoc yasm_arch_segreg_print() */
    void (*segreg_print) (yasm_arch *arch, unsigned long segreg, FILE *f);

    /** \copydoc yasm_arch_ea_create() */
    yasm_effaddr * (*ea_create) (yasm_arch *arch, /*@keep@*/ yasm_expr *e);

    /** NULL-terminated list of machines for this architecture. */
    yasm_arch_machine *machines;

    /** Default machine keyword. */
    const char *default_machine_keyword;

    /** Canonical "word" size in bytes. */
    unsigned int wordsize;
};
#endif

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

/** Get the version number of an architecture module.
 * \param module	architecture module
 * \return Version number (see #YASM_ARCH_VERSION).
 */
unsigned int yasm_arch_module_version(const yasm_arch_module *module);

/** Get the machines usable with an architecture module.
 * \param module	architecture module
 * \return NULL-terminated list of machines.
 */
const yasm_arch_machine *yasm_arch_module_machines
    (const yasm_arch_module *module);

/** Get the default machine keyword for an architecture module.
 * \param module	architecture module
 * \return Default machine keyword.
 */
const char *yasm_arch_module_def_machine(const yasm_arch_module *module);

/** Get the one-line description of an architecture.
 * \param arch	    architecture
 * \return keyword
 */
const char *yasm_arch_name(const yasm_arch *arch);

/** Get the keyword used to select an architecture.
 * \param arch	    architecture
 * \return keyword
 */
const char *yasm_arch_keyword(const yasm_arch *arch);

/** Get the word size of an architecture.
 * \param arch	    architecture
 * \return word size
 */
unsigned int yasm_arch_wordsize(const yasm_arch *arch);

/** Create architecture.
 * \param module	architecture module
 * \param machine	keyword of machine in use (must be one listed in
 *			#machines)
 * \return NULL if machine not recognized, otherwise new architecture.
 */
/*@only@*/ yasm_arch *yasm_arch_create(const yasm_arch_module *module,
				       const char *machine);

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
 * names for instructions, registers, etc.  Unrecognized identifiers should
 * be returned as #YASM_ARCH_CHECK_ID_NONE so they can be treated as normal
 * symbols.  Any additional data beyond just the type (almost always
 * necessary) should be returned into the space provided by the data
 * parameter.
 * \note Even though this is passed a data[4], only data[0] should be used
 *	 for #YASM_ARCH_CHECK_ID_TARGETMOD, #YASM_ARCH_CHECK_ID_REG, and
 *	 #YASM_ARCH_CHECK_ID_SEGREG return values.
 * \param arch	architecture
 * \param data	extra identification information (yasm_arch-specific)
 *		[output]
 * \param id	identifier as in the input file
 * \param line	virtual line (from yasm_linemap)
 * \return General type of identifier (#yasm_arch_check_id_retval).
 */
yasm_arch_check_id_retval yasm_arch_parse_check_id
    (yasm_arch *arch, unsigned long data[4], const char *id,
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

/** Create an instruction.  Creates a bytecode by matching the
 * instruction data and the parameters given with a valid instruction.
 * \param arch		architecture
 * \param data		instruction data (from parse_check_id()); all
 *				zero indicates an empty instruction
 * \param num_operands	number of operands parsed
 * \param operands	list of operands (in parse order)
 * \param prev_bc	previously parsed bytecode in section (NULL if
 *			first bytecode in section)
 * \param line		virtual line (from yasm_linemap)
 * \return If no match is found (the instruction is invalid), NULL,
 *	       otherwise newly allocated bytecode containing instruction.
 */
/*@null@*/ yasm_bytecode *yasm_arch_parse_insn
    (yasm_arch *arch, const unsigned long data[4], int num_operands,
     /*@null@*/ yasm_insn_operands *operands, yasm_bytecode *prev_bc,
     unsigned long line);

/** Handle an instruction prefix.
 * Modifies an instruction bytecode based on the prefix in data.
 * \param arch	architecture
 * \param bc	bytecode (must be instruction bytecode)
 * \param data	prefix (from parse_check_id())
 * \param line	virtual line (from yasm_linemap)
 */
void yasm_arch_parse_prefix(yasm_arch *arch, yasm_bytecode *bc,
			    const unsigned long data[4], unsigned long line);

/** Handle an segment register instruction prefix.
 * Modifies an instruction bytecode based on a segment register prefix.
 * \param arch		architecture
 * \param bc		bytecode (must be instruction bytecode)
 * \param segreg	segment register (from parse_check_id())
 * \param line		virtual line (from yasm_linemap)
 */
void yasm_arch_parse_seg_prefix(yasm_arch *arch, yasm_bytecode *bc,
				unsigned long segreg, unsigned long line);

/** Handle a memory expression segment override.
 * Modifies an instruction bytecode based on a segment override in a
 * memory expression.
 * \param arch		architecture
 * \param ea		effective address
 * \param segreg	segment register (from parse_check_id())
 * \param line		virtual line (from yasm_linemap)
 */
void yasm_arch_parse_seg_override(yasm_arch *arch, yasm_effaddr *ea,
				  unsigned long segreg, unsigned long line);

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
 * \param rel		value is a relative displacement from bc
 * \param warn		enables standard warnings (value doesn't fit into
 *			valsize bits)
 * \param line		virtual line; may be 0 if warn is 0
 * \return Nonzero on error.
 */
int yasm_arch_intnum_tobytes(yasm_arch *arch, const yasm_intnum *intn,
			     unsigned char *buf, size_t destsize,
			     size_t valsize, int shift,
			     const yasm_bytecode *bc, int rel, int warn,
			     unsigned long line);

/** Get the equivalent byte size of a register.
 * \param arch	architecture
 * \param reg	register
 * \return 0 if there is no suitable equivalent size, otherwise the size.
 */
unsigned int yasm_arch_get_reg_size(yasm_arch *arch, unsigned long reg);

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

#define yasm_arch_module_version(module, machine)   (module->version)
#define yasm_arch_module_machines(module, machine)  (module->machines)
#define yasm_arch_module_def_machine(module, machine) \
    (module->default_machine_keyword)

#define yasm_arch_name(arch) \
    (((yasm_arch_base *)arch)->module->name)
#define yasm_arch_keyword(arch) \
    (((yasm_arch_base *)arch)->module->keyword)
#define yasm_arch_wordsize(arch) \
    (((yasm_arch_base *)arch)->module->wordsize)

#define yasm_arch_create(module, machine)   module->create(machine)

#define yasm_arch_destroy(arch) \
    ((yasm_arch_base *)arch)->module->destroy(arch)
#define yasm_arch_get_machine(arch) \
    ((yasm_arch_base *)arch)->module->get_machine(arch)
#define yasm_arch_set_var(arch, var, val) \
    ((yasm_arch_base *)arch)->module->set_var(arch, var, val)
#define yasm_arch_parse_cpu(arch, cpuid, line) \
    ((yasm_arch_base *)arch)->module->parse_cpu(arch, cpuid, line)
#define yasm_arch_parse_check_id(arch, data, id, line) \
    ((yasm_arch_base *)arch)->module->parse_check_id(arch, data, id, line)
#define yasm_arch_parse_directive(arch, name, valparams, objext_valparams, \
				  object, line) \
    ((yasm_arch_base *)arch)->module->parse_directive \
	(arch, name, valparams, objext_valparams, object, line)
#define yasm_arch_parse_insn(arch, data, num_operands, operands, prev_bc, \
			     line) \
    ((yasm_arch_base *)arch)->module->parse_insn \
	(arch, data, num_operands, operands, prev_bc, line)
#define yasm_arch_parse_prefix(arch, bc, data, line) \
    ((yasm_arch_base *)arch)->module->parse_prefix(arch, bc, data, line)
#define yasm_arch_parse_seg_prefix(arch, bc, segreg, line) \
    ((yasm_arch_base *)arch)->module->parse_seg_prefix(arch, bc, segreg, line)
#define yasm_arch_parse_seg_override(arch, ea, segreg, line) \
    ((yasm_arch_base *)arch)->module->parse_seg_override \
	(arch, ea, segreg, line)
#define yasm_arch_floatnum_tobytes(arch, flt, buf, destsize, valsize, shift, \
				   warn, line) \
    ((yasm_arch_base *)arch)->module->floatnum_tobytes \
	(arch, flt, buf, destsize, valsize, shift, warn, line)
#define yasm_arch_intnum_tobytes(arch, intn, buf, destsize, valsize, shift, \
				 bc, rel, warn, line) \
    ((yasm_arch_base *)arch)->module->intnum_tobytes \
	(arch, intn, buf, destsize, valsize, shift, bc, rel, warn, line)
#define yasm_arch_get_reg_size(arch, reg) \
    ((yasm_arch_base *)arch)->module->get_reg_size(arch, reg)
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
