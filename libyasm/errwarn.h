/**
 * \file errwarn.h
 * \brief YASM error and warning reporting interface.
 *
 * $IdPath: yasm/libyasm/errwarn.h,v 1.35 2003/03/16 23:52:54 peter Exp $
 *
 *  Copyright (C) 2001  Peter Johnson
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
 */
#ifndef YASM_ERRWARN_H
#define YASM_ERRWARN_H

/** Warning classes (that may be enabled/disabled). */
typedef enum {
    YASM_WARN_GENERAL = 0,  /**< Non-specific warnings */
    YASM_WARN_UNREC_CHAR,   /**< Unrecognized characters (while tokenizing) */
    YASM_WARN_PREPROC	    /**< Preprocessor warnings */
} yasm_warn_class;

/** Initialize any internal data structures. */
void yasm_errwarn_initialize(void);

/** Clean up any memory allocated by yasm_errwarn_initialize() or other
 * functions.
 */
void yasm_errwarn_cleanup(void);

/** Reporting point of internal errors.  These are usually due to sanity
 * check failures in the code.
 * \warning This function must NOT return to calling code; exit or longjmp
 *          instead.
 * \param file      source file (ala __FILE__)
 * \param line      source line (ala __LINE__)
 * \param message   internal error message
 */
extern /*@exits@*/ void (*yasm_internal_error_)
    (const char *file, unsigned int line, const char *message);

/** Easily-callable version of yasm_internal_error_().  Automatically uses
 * __FILE__ and __LINE__ as the file and line.
 * \param message   internal error message
 */
#define yasm_internal_error(message) \
    yasm_internal_error_(__FILE__, __LINE__, message)

/** Reporting point of fatal errors.
 * \warning This function must NOT return to calling code; exit or longjmp
 *          instead.
 * \param message   fatal error message
 */
extern /*@exits@*/ void (*yasm_fatal) (const char *message);

#ifdef YASM_INTERNAL

/** \internal Log an error.  va_list version of yasm__error().
 * \param lindex    line index
 * \param message   printf-like-format message
 * \param va	    argument list for message
 */
void yasm__error_va(unsigned long lindex, const char *message, va_list va);

/** \internal Log a warning.  va_list version of yasm__warning().
 * \param wclass    warning class
 * \param lindex    line index
 * \param message   printf-like-format message
 * \param va	    argument list for message
 */
void yasm__warning_va(yasm_warn_class wclass, unsigned long lindex,
		      const char *message, va_list va);

/** \internal Log an error.  Does not print it out immediately;
 * yasm_errwarn_output_all() outputs errors and warnings.
 * \param lindex    line index
 * \param message   printf-like-format message
 * \param ...	    argument list for message
 */
void yasm__error(unsigned long lindex, const char *message, ...)
    /*@printflike@*/;

/** \internal Log a warning.  Does not print it out immediately;
 * yasm_errwarn_output_all() outputs errors and warnings.
 * \param wclass    warning class
 * \param lindex    line index
 * \param message   printf-like-format message
 * \param ...	    argument list for message
 */
void yasm__warning(yasm_warn_class, unsigned long lindex, const char *message,
		   ...) /*@printflike@*/;

/** \internal Log a parser error.  Parser errors can be overwritten by
 * non-parser errors on the same line.
 * \param lindex    line index
 * \param message   parser error message
 */
void yasm__parser_error(unsigned long lindex, const char *message);

#endif

/** Enable a class of warnings.
 * \param wclass    warning class
 */
void yasm_warn_enable(yasm_warn_class wclass);

/** Disable a class of warnings.
 * \param wclass    warning class
 */
void yasm_warn_disable(yasm_warn_class wclass);

/** Disable all classes of warnings. */
void yasm_warn_disable_all(void);

/** Get total number of errors logged.
 * \param warning_as_error  if nonzero, warnings are treated as errors.
 * \return Number of errors.
 */
unsigned int yasm_get_num_errors(int warning_as_error);

/** Print out an error.
 * \param fn	filename of source file
 * \param line	line number
 * \param msg	error message
 */
typedef void (*yasm_print_error_func)
    (const char *fn, unsigned long line, const char *msg);

/** Print out a warning.
 * \param fn	filename of source file
 * \param line	line number
 * \param msg	warning message
 */
typedef void (*yasm_print_warning_func)
    (const char *fn, unsigned long line, const char *msg);

/** Outputs all errors and warnings.
 * \param lm	line manager (to convert line indexes into filename/line pairs)
 * \param warning_as_error  if nonzero, treat warnings as errors
 * \param print_error	    function called to print out errors
 * \param print_warning	    function called to print out warnings
 */
void yasm_errwarn_output_all
    (yasm_linemgr *lm, int warning_as_error, yasm_print_error_func print_error,
     yasm_print_warning_func print_warning);

#ifdef YASM_INTERNAL
/** \internal Convert a possibly unprintable character into a printable string.
 * \param ch	possibly unprintable character
 * \return Printable string representation (static buffer).
 */
char *yasm__conv_unprint(int ch);
#endif

/** Hook for library users to map to gettext() if GNU gettext is being used.
 * \param msgid	    message catalog identifier
 * \return Translated message.
 */
extern const char * (*yasm_gettext_hook) (const char *msgid);

#endif
