# Python bindings for Yasm: Pyrex input file for coretype.h
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

cdef extern struct FILE:
    int fileno

ctypedef unsigned long size_t

cdef extern from "libyasm/coretype.h":
    cdef struct yasm_arch
    cdef struct yasm_preproc
    cdef struct yasm_parser
    cdef struct yasm_optimizer
    cdef struct yasm_objfmt
    cdef struct yasm_dbgfmt
    cdef struct yasm_listfmt

    cdef struct yasm_assoc_data_callback:
        void (*destroy) (void *data)
        void (*print_ "print") (void *data, FILE *f, int indent_level)

    cdef struct yasm_bytecode
    cdef struct yasm_object
    cdef struct yasm_section
    cdef struct yasm_symtab
    cdef struct yasm_symrec
    cdef struct yasm_expr
    cdef struct yasm_intnum
    cdef struct yasm_floatnum

    cdef struct yasm_value:
        yasm_expr *abs
        yasm_symrec *rel
        yasm_symrec *wrt
        unsigned int seg_of
        unsigned int rshift
        unsigned int curpos_rel
        unsigned int ip_rel
        unsigned int section_rel

    cdef struct yasm_linemap
    cdef struct yasm_valparam
    cdef struct yasm_valparamhead
    cdef struct yasm_insn_operands

    ctypedef enum yasm_expr_op:
        YASM_EXPR_IDENT
        YASM_EXPR_ADD
        YASM_EXPR_SUB
        YASM_EXPR_MUL
        YASM_EXPR_DIV
        YASM_EXPR_SIGNDIV
        YASM_EXPR_MOD
        YASM_EXPR_SIGNMOD
        YASM_EXPR_NEG
        YASM_EXPR_NOT
        YASM_EXPR_OR
        YASM_EXPR_AND
        YASM_EXPR_XOR
        YASM_EXPR_XNOR
        YASM_EXPR_NOR
        YASM_EXPR_SHL
        YASM_EXPR_SHR
        YASM_EXPR_LOR
        YASM_EXPR_LAND
        YASM_EXPR_LNOT
        YASM_EXPR_LXOR
        YASM_EXPR_LXNOR
        YASM_EXPR_LNOR
        YASM_EXPR_LT
        YASM_EXPR_GT
        YASM_EXPR_EQ
        YASM_EXPR_LE
        YASM_EXPR_GE
        YASM_EXPR_NE
        YASM_EXPR_NONNUM
        YASM_EXPR_SEG
        YASM_EXPR_WRT
        YASM_EXPR_SEGOFF

    ctypedef enum yasm_sym_vis:
        YASM_SYM_LOCAL
        YASM_SYM_GLOBAL
        YASM_SYM_COMMON
        YASM_SYM_EXTERN
        YASM_SYM_DLOCAL

    ctypedef yasm_intnum*(*yasm_calc_bc_dist_func)(yasm_bytecode *precbc1,
            yasm_bytecode *precbc2)
    ctypedef int*(*yasm_output_value_func)(yasm_value *value, unsigned char
            *buf, size_t destsize, size_t valsize, int shift, unsigned
            long offset, yasm_bytecode *bc, int warn, void *d)
    ctypedef int(*yasm_output_reloc_func)(yasm_symrec *sym,
            yasm_bytecode *bc, unsigned char *buf, size_t destsize,
            size_t valsize, int warn, void *d)

