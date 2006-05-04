# Python bindings for Yasm: Pyrex input file for bytecode.h
#
#  Copyright (C) 2006  Michael Urman, Peter Johnson
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

cdef extern from "libyasm/bytecode.h":
    cdef struct yasm_effaddr

    cdef struct yasm_effaddr_callback:
        void (*destroy) (yasm_effaddr *ea)
        void (*print_ "print") (yasm_effaddr *ea, FILE *f, int indent_level)

    cdef struct yasm_effaddr:
        yasm_effaddr_callback *callback
        yasm_value disp
        unsigned long segreg
        unsigned int disp_len
        unsigned int need_disp
        unsigned int nosplit
        unsigned int strong

    cdef struct yasm_immval:
        yasm_value val
        unsigned int len
        unsigned int sign

    cdef struct yasm_dataval
    cdef struct yasm_datavalhead

    cdef enum yasm_bc_resolve_flags:
        YASM_BC_RESOLVE_NONE
        YASM_BC_RESOLVE_ERROR
        YASM_BC_RESOLVE_MIN_LEN
        YASM_BC_RESOLVE_UNKNOWN_LEN

    cdef yasm_immval* yasm_imm_create_expr(yasm_expr *e)
    cdef yasm_expr* yasm_ea_get_disp(yasm_effaddr *ea)
    cdef void yasm_ea_set_len(yasm_effaddr *ea, unsigned int len)
    cdef void yasm_ea_set_nosplit(yasm_effaddr *ea, unsigned int nosplit)
    cdef void yasm_ea_set_strong(yasm_effaddr *ea, unsigned int strong)
    cdef void yasm_ea_set_segreg(yasm_effaddr *ea, unsigned long segreg)
    cdef void yasm_ea_destroy(yasm_effaddr *ea)
    cdef void yasm_ea_print(yasm_effaddr *ea, FILE *f, int indent_level)

    cdef void yasm_bc_set_multiple(yasm_bytecode *bc, yasm_expr *e)
    cdef yasm_bytecode* yasm_bc_create_data(yasm_datavalhead *datahead,
            unsigned int size, int append_zero, unsigned long line)
    cdef yasm_bytecode* yasm_bc_create_leb128(yasm_datavalhead *datahead,
            int sign, unsigned long line)
    cdef yasm_bytecode* yasm_bc_create_reserve(yasm_expr *numitems,
            unsigned int itemsize, unsigned long line)
    cdef yasm_bytecode* yasm_bc_create_incbin(char *filename,
            yasm_expr *start, yasm_expr *maxlen, unsigned long line)
    cdef yasm_bytecode* yasm_bc_create_align(yasm_expr *boundary,
            yasm_expr *fill, yasm_expr *maxskip,
            unsigned char **code_fill, unsigned long line)
    cdef yasm_bytecode* yasm_bc_create_org(unsigned long start,
            unsigned long line)
    cdef yasm_bytecode* yasm_bc_create_insn(yasm_arch *arch,
            unsigned long insn_data[4], int num_operands,
            yasm_insn_operands *operands, unsigned long line)
    cdef yasm_bytecode* yasm_bc_create_empty_insn(yasm_arch *arch,
            unsigned long insn_data[4], int num_operands,
            yasm_insn_operands *operands, unsigned long line)
    cdef void yasm_bc_insn_add_prefix(yasm_bytecode *bc,
            unsigned long prefix_data[4])
    cdef void yasm_bc_insn_add_seg_prefix(yasm_bytecode *bc,
            unsigned long segreg)
    cdef yasm_section* yasm_bc_get_section(yasm_bytecode *bc)
    cdef void yasm_bc__add_symrec(yasm_bytecode *bc, yasm_symrec *sym)
    cdef void yasm_bc_destroy(yasm_bytecode *bc)
    cdef void yasm_bc_print(yasm_bytecode *bc, FILE *f, int indent_level)
    cdef void yasm_bc_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc)
    cdef yasm_intnum *yasm_common_calc_bc_dist(yasm_bytecode *precbc1,
            yasm_bytecode *precbc2)
    cdef yasm_bc_resolve_flags yasm_bc_resolve(yasm_bytecode *bc, int save,
            yasm_calc_bc_dist_func calc_bc_dist)
    cdef unsigned char* yasm_bc_tobytes(yasm_bytecode *bc,
            unsigned char *buf, unsigned long *bufsize,
            unsigned long *multiple, int *gap, void *d,
            yasm_output_value_func output_value,
            yasm_output_reloc_func output_reloc)

    cdef yasm_dataval* yasm_dv_create_expr(yasm_expr *expn)
    cdef yasm_dataval* yasm_dv_create_string(char *contents, size_t len)

    cdef void yasm_dvs_initialize(yasm_datavalhead *headp)
    cdef void yasm_dvs_destroy(yasm_datavalhead *headp)
    cdef yasm_dataval* yasm_dvs_append(yasm_datavalhead *headp,
            yasm_dataval *dv)
    cdef void yasm_dvs_print(yasm_datavalhead *headp, FILE *f,
            int indent_level)

cdef extern from "libyasm/bc-int.h":
    cdef struct yasm_bytecode_callback:
        void (*destroy) (void *contents)
        void (*c_print "print") (void *contents, FILE *f, int indent_level)
        void (*finalize) (yasm_bytecode *bc, yasm_bytecode *prev_bc)
        yasm_bc_resolve_flags (*resolve) (yasm_bytecode *bc, int save,
                                          yasm_calc_bc_dist_func calc_bc_dist)
        int (*tobytes) (yasm_bytecode *bc, unsigned char **bufp, void *d,
	                yasm_output_value_func output_value,
		        yasm_output_reloc_func output_reloc)

    cdef struct yasm_bytecode:
        yasm_bytecode_callback *callback
        yasm_section *section
        yasm_expr *multiple
        unsigned long len
        unsigned long line
        unsigned long offset
        unsigned long opt_flags
        yasm_symrec **symrecs
        void *contents

    cdef yasm_bytecode *yasm_bc_create_common(yasm_bytecode_callback *callback,
            void *contents, unsigned long line)

    cdef void yasm_bc_transform(yasm_bytecode *bc,
            yasm_bytecode_callback *callback, void *contents)

    cdef void yasm_bc_finalize_common(yasm_bytecode *bc, yasm_bytecode *prev_bc)

    cdef yasm_bytecode *yasm_bc__next(yasm_bytecode *bc)

cdef class Bytecode:
    cdef yasm_bytecode *bc

    property len:
        def __get__(self): return self.bc.len
        def __set__(self, value): self.bc.len = value
    property line:
        def __get__(self): return self.bc.line
        def __set__(self, value): self.bc.line = value
    property offset:
        def __get__(self): return self.bc.offset
        def __set__(self, value): self.bc.offset = value
    property opt_flags:
        def __get__(self): return self.bc.opt_flags
        def __set__(self, value): self.bc.opt_flags = value
    property symbols:
        # Someday extend this to do something modifiable, e.g. return a
        # list-like object.
        def __get__(self):
            cdef yasm_symrec *sym
            cdef int i
            s = []
            i = 0
            sym = self.bc.symrecs[i]
            while sym != NULL:
                s.append(__make_symbol(sym))
                i = i+1
                sym = self.bc.symrecs[i]
            return s
