/* $IdPath$
 * YASM error and warning reporting and related functions header file.
 *
 *  Copyright (C) 2001  Peter Johnson
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
#ifndef YASM_ERRWARN_H
#define YASM_ERRWARN_H

/* Warning classes (may be enabled/disabled). */
typedef enum {
    YASM_WARN_GENERAL = 0,  /* non-specific warnings */
    YASM_WARN_UNREC_CHAR,   /* unrecognized characters (while tokenizing) */
    YASM_WARN_PREPROC	    /* preprocessor warnings */
} yasm_warn_class;

/* Fatal error constants.
 * See fatal_msgs in errwarn.c for strings that match up to these constants.
 * When adding a constant here, keep errwarn.c in sync!
 */
typedef enum {
    YASM_FATAL_UNKNOWN = 0,
    YASM_FATAL_NOMEM
} yasm_fatal_cause;

struct yasm_errwarn {
    /* Initialize any internal data structures. */
    void (*initialize) (void);

    /* Cleans up any memory allocated by initialize or other functions. */
    void (*cleanup) (void);

    /* Reporting point of internal errors.  These are usually due to sanity
     * check failures in the code.
     * This function must NOT return to calling code.  Either exit or longjmp.
     */
    /*@exits@*/ void (*internal_error_) (const char *file, unsigned int line,
					 const char *message);
#define internal_error(msg)	internal_error_(__FILE__, __LINE__, msg)

    /* Reporting point of fatal errors.
     * This function must NOT return to calling code.  Either exit or longjmp.
     */
    /*@exits@*/ void (*fatal) (yasm_fatal_cause);

    /* va_list versions of the below two functions */
    void (*error_va) (unsigned long lindex, const char *, va_list va);
    void (*warning_va) (yasm_warn_class, unsigned long lindex, const char *,
			va_list va);

    void (*error) (unsigned long lindex, const char *, ...) /*@printflike@*/;
    void (*warning) (yasm_warn_class, unsigned long lindex, const char *, ...)
	/*@printflike@*/;

    /* Logs a parser error.  These can be overwritten by non-parser errors on
     * the same line.
     */
    void (*parser_error) (unsigned long lindex, const char *);

    /* Enables/disables a class of warnings. */
    void (*warn_enable) (yasm_warn_class);
    void (*warn_disable) (yasm_warn_class);
    void (*warn_disable_all) (void);

    /* Returns total number of errors logged to this point.
     * If warning_as_error is nonzero, warnings are treated as errors.
     */
    unsigned int (*get_num_errors) (int warning_as_error);

    /* Outputs all errors/warnings (e.g. to stderr or elsewhere). */
    void (*output_all) (yasm_linemgr *lm, int warning_as_error);

    /* Convert a possibly unprintable character into a printable string. */
    char * (*conv_unprint) (char ch);
};

extern yasm_errwarn yasm_std_errwarn;

#endif
