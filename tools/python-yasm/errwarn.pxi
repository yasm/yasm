# Python bindings for Yasm: Pyrex input file for errwarn.h
#
#  Copyright (C) 2006  Peter Johnson
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

cdef extern from "libyasm/errwarn.h":
    ctypedef enum yasm_warn_class:
        YASM_WARN_NONE
        YASM_WARN_GENERAL
        YASM_WARN_UNREC_CHAR
        YASM_WARN_PREPROC
        YASM_WARN_ORPHAN_LABEL
        YASM_WARN_UNINIT_CONTENTS

    ctypedef enum yasm_error_class:
        YASM_ERROR_NONE
        YASM_ERROR_GENERAL
        YASM_ERROR_ARITHMETIC
        YASM_ERROR_OVERFLOW
        YASM_ERROR_FLOATING_POINT
        YASM_ERROR_ZERO_DIVISION
        YASM_ERROR_ASSERTION
        YASM_ERROR_VALUE
        YASM_ERROR_NOT_ABSOLUTE
        YASM_ERROR_TOO_COMPLEX
        YASM_ERROR_NOT_CONSTANT
        YASM_ERROR_IO
        YASM_ERROR_NOT_IMPLEMENTED
        YASM_ERROR_TYPE
        YASM_ERROR_SYNTAX
        YASM_ERROR_PARSE

    void yasm_errwarn_initialize()
    void yasm_errwarn_cleanup()
    extern void (*yasm_internal_error_) (char *file, unsigned int line,
                                         char *message)
    void yasm_internal_error(char *message)
    extern void (*yasm_fatal) (char *message, va_list va)
    void yasm__fatal(char *message, ...)

    void yasm_error_clear()
    yasm_error_class yasm_error_occurred()
    int yasm_error_matches(yasm_error_class eclass)

    void yasm_error_set_va(yasm_error_class eclass, char *format, va_list va)
    void yasm_error_set(yasm_error_class eclass, char *format, ...)
    void yasm_error_set_xref_va(unsigned long xrefline, char *format,
                                va_list va)
    void yasm_error_set_xref(unsigned long xrefline, char *format, ...)
    void yasm_error_fetch(yasm_error_class *eclass, char **str,
                          unsigned long *xrefline, char **xrefstr)

    void yasm_warn_clear()
    void yasm_warn_set_va(yasm_warn_class wclass, char *format, va_list va)
    void yasm_warn_set(yasm_warn_class wclass, char *format, ...)
    void yasm_warn_fetch(yasm_warn_class *wclass, char **str)

    void yasm_warn_enable(yasm_warn_class wclass)
    void yasm_warn_disable(yasm_warn_class wclass)

    void yasm_warn_disable_all()

    yasm_errwarns *yasm_errwarns_create()
    void yasm_errwarns_destroy(yasm_errwarns *errwarns)
    void yasm_errwarn_propagate(yasm_errwarns *errwarns, unsigned long line)
    unsigned int yasm_errwarns_num_errors(yasm_errwarns *errwarns,
                                          int warning_as_error)

    ctypedef void (*yasm_print_error_func) (char *fn, unsigned long line,
                                            char *msg, unsigned long xrefline,
                                            char *xrefmsg)
    ctypedef void (*yasm_print_warning_func) (char *fn, unsigned long line,
                                              char *msg)
    void yasm_errwarns_output_all(yasm_errwarns *errwarns, yasm_linemap *lm,
                                  int warning_as_error,
                                  yasm_print_error_func print_error,
                                  yasm_print_warning_func print_warning)

    char *yasm__conv_unprint(int ch)

    extern char * (*yasm_gettext_hook) (char *msgid)

class YasmError(Exception): pass

__errormap = [
    # Order matters here. Go from most to least specific within a class
    (YASM_ERROR_ZERO_DIVISION, ZeroDivisionError),
    # Enable these once there are tests that need them.
    #(YASM_ERROR_OVERFLOW, OverflowError),
    #(YASM_ERROR_FLOATING_POINT, FloatingPointError),
    #(YASM_ERROR_ARITHMETIC, ArithmeticError),
    #(YASM_ERROR_ASSERTION, AssertionError),
    #(YASM_ERROR_VALUE, ValueError), # include notabs, notconst, toocomplex
    #(YASM_ERROR_IO, IOError),
    #(YASM_ERROR_NOT_IMPLEMENTED, NotImplementedError),
    #(YASM_ERROR_TYPE, TypeError),
    #(YASM_ERROR_SYNTAX, SyntaxError), #include parse
]

cdef void __error_check() except *:
    cdef yasm_error_class errclass
    cdef unsigned long xrefline
    cdef char *errstr, *xrefstr

    # short path for the common case
    if not yasm_error_occurred(): return

    # look up our preferred python error, fall back to YasmError
    for error_class, exception in __errormap:
        if yasm_error_matches(error_class):
            break
    else:
        exception = YasmError

    # retrieve info (clears error)
    yasm_error_fetch(&errclass, &errstr, &xrefline, &xrefstr)

    if xrefline and xrefstr:
        PyErr_Format(exception, "%s: %d: %s", errstr, xrefline, xrefstr)
    else:
        PyErr_SetString(exception, errstr)

    if xrefstr: free(xrefstr)
    free(errstr)
