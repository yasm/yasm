/*
 * Error and warning reporting and related functions.
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
#define YASM_LIB_INTERNAL
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include <ctype.h>

#ifdef STDC_HEADERS
# include <stdarg.h>
#endif

#include "coretype.h"

#include "linemgr.h"
#include "errwarn.h"


#define MSG_MAXSIZE	1024

/* Default handlers for replacable functions */
static /*@exits@*/ void def_internal_error_
    (const char *file, unsigned int line, const char *message);
static /*@exits@*/ void def_fatal(const char *message, ...);
static const char *def_gettext_hook(const char *msgid);

/* Storage for errwarn's "extern" functions */
/*@exits@*/ void (*yasm_internal_error_)
    (const char *file, unsigned int line, const char *message)
    = def_internal_error_;
/*@exits@*/ void (*yasm_fatal) (const char *message, ...) = def_fatal;
const char * (*yasm_gettext_hook) (const char *msgid) = def_gettext_hook;

/* Enabled warnings.  See errwarn.h for a list. */
static unsigned long warn_class_enabled;

/* Total error count */
static unsigned int error_count;

/* Total warning count */
static unsigned int warning_count;

typedef /*@reldef@*/ SLIST_HEAD(errwarndatahead_s, errwarn_data)
    errwarndatahead;
static /*@only@*/ /*@null@*/ errwarndatahead errwarns;

typedef struct errwarn_data {
    /*@reldef@*/ SLIST_ENTRY(errwarn_data) link;

    enum { WE_UNKNOWN, WE_ERROR, WE_WARNING, WE_PARSERERROR } type;

    unsigned long line;
    /* FIXME: This should not be a fixed size.  But we don't have vasprintf()
     * right now. */
    char msg[MSG_MAXSIZE];
} errwarn_data;

/* Last inserted error/warning.  Used to speed up insertions. */
static /*@null@*/ errwarn_data *previous_we;

/* Static buffer for use by conv_unprint(). */
static char unprint[5];


static const char *
def_gettext_hook(const char *msgid)
{
    return msgid;
}

void
yasm_errwarn_initialize(void)
{
    /* Default enabled warnings.  See errwarn.h for a list. */
    warn_class_enabled = 
	(1UL<<YASM_WARN_GENERAL) | (1UL<<YASM_WARN_UNREC_CHAR) |
	(1UL<<YASM_WARN_PREPROC);

    error_count = 0;
    warning_count = 0;
    SLIST_INIT(&errwarns);
    previous_we = NULL;
}

void
yasm_errwarn_cleanup(void)
{
    errwarn_data *we;

    /* Delete all error/warnings */
    while (!SLIST_EMPTY(&errwarns)) {
	we = SLIST_FIRST(&errwarns);

	SLIST_REMOVE_HEAD(&errwarns, link);
	yasm_xfree(we);
    }
}

/* Convert a possibly unprintable character into a printable string, using
 * standard cat(1) convention for unprintable characters.
 */
char *
yasm__conv_unprint(int ch)
{
    int pos = 0;

    if (((ch & ~0x7F) != 0) /*!isascii(ch)*/ && !isprint(ch)) {
	unprint[pos++] = 'M';
	unprint[pos++] = '-';
	ch &= toascii(ch);
    }
    if (iscntrl(ch)) {
	unprint[pos++] = '^';
	unprint[pos++] = (ch == '\177') ? '?' : ch | 0100;
    } else
	unprint[pos++] = ch;
    unprint[pos] = '\0';

    return unprint;
}

/* Report an internal error.  Essentially a fatal error with trace info.
 * Exit immediately because it's essentially an assert() trap.
 */
static void
def_internal_error_(const char *file, unsigned int line, const char *message)
{
    fprintf(stderr,
	    yasm_gettext_hook(N_("INTERNAL ERROR at %s, line %u: %s\n")),
	    file, line, yasm_gettext_hook(message));
#ifdef HAVE_ABORT
    abort();
#else
    exit(EXIT_FAILURE);
#endif
}

/* Report a fatal error.  These are unrecoverable (such as running out of
 * memory), so just exit immediately.
 */
static void
def_fatal(const char *fmt, ...)
{
    va_list va;
    fprintf(stderr, "%s: ", yasm_gettext_hook(N_("FATAL")));
    va_start(va, fmt);
    vfprintf(stderr, yasm_gettext_hook(fmt), va);
    va_end(va);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

/* Create an errwarn structure in the correct linked list location.
 * If replace_parser_error is nonzero, overwrites the last error if its
 * type is WE_PARSERERROR.
 */
static errwarn_data *
errwarn_data_new(unsigned long line, int replace_parser_error)
{
    errwarn_data *first, *next, *ins_we, *we;
    enum { INS_NONE, INS_HEAD, INS_AFTER } action = INS_NONE;

    /* Find the entry with either line=line or the last one with line<line.
     * Start with the last entry added to speed the search.
     */
    ins_we = previous_we;
    first = SLIST_FIRST(&errwarns);
    if (!ins_we || !first)
	action = INS_HEAD;
    while (action == INS_NONE) {
	next = SLIST_NEXT(ins_we, link);
	if (line < ins_we->line) {
	    if (ins_we == first)
		action = INS_HEAD;
	    else
		ins_we = first;
	} else if (!next)
	    action = INS_AFTER;
	else if (line >= ins_we->line && line < next->line)
	    action = INS_AFTER;
	else
	    ins_we = next;
    }

    if (replace_parser_error && ins_we && ins_we->type == WE_PARSERERROR) {
	/* overwrite last error */	
	we = ins_we;
    } else {
	/* add a new error */
	we = yasm_xmalloc(sizeof(errwarn_data));

	we->type = WE_UNKNOWN;
	we->line = line;

	if (action == INS_HEAD)
	    SLIST_INSERT_HEAD(&errwarns, we, link);
	else if (action == INS_AFTER) {
	    assert(ins_we != NULL);
	    SLIST_INSERT_AFTER(ins_we, we, link);
	} else
	    yasm_internal_error(N_("Unexpected errwarn insert action"));
    }

    /* Remember previous err/warn */
    previous_we = we;

    return we;
}

/* Register an error at line line.  Does not print the error, only stores it
 * for output_all() to print.
 */
void
yasm__error_va(unsigned long line, const char *fmt, va_list va)
{
    errwarn_data *we = errwarn_data_new(line, 1);

    we->type = WE_ERROR;

#ifdef HAVE_VSNPRINTF
    vsnprintf(we->msg, MSG_MAXSIZE, yasm_gettext_hook(fmt), va);
#else
    vsprintf(we->msg, yasm_gettext_hook(fmt), va);
#endif

    error_count++;
}

/* Register an warning at line line.  Does not print the warning, only stores
 * it for output_all() to print.
 */
void
yasm__warning_va(yasm_warn_class num, unsigned long line, const char *fmt,
		 va_list va)
{
    errwarn_data *we;

    if (!(warn_class_enabled & (1UL<<num)))
	return;	    /* warning is part of disabled class */

    we = errwarn_data_new(line, 0);

    we->type = WE_WARNING;

#ifdef HAVE_VSNPRINTF
    vsnprintf(we->msg, MSG_MAXSIZE, yasm_gettext_hook(fmt), va);
#else
    vsprintf(we->msg, yasm_gettext_hook(fmt), va);
#endif

    warning_count++;
}

/* Register an error at line line.  Does not print the error, only stores it
 * for output_all() to print.
 */
void
yasm__error(unsigned long line, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    yasm__error_va(line, fmt, va);
    va_end(va);
}

/* Register an warning at line line.  Does not print the warning, only stores
 * it for output_all() to print.
 */
void
yasm__warning(yasm_warn_class num, unsigned long line, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    yasm__warning_va(num, line, fmt, va);
    va_end(va);
}

/* Parser error handler.  Moves YACC-style error into our error handling
 * system.
 */
void
yasm__parser_error(unsigned long line, const char *s)
{
    yasm__error(line, N_("parser error: %s"), s);
    previous_we->type = WE_PARSERERROR;
}

void
yasm_warn_enable(yasm_warn_class num)
{
    warn_class_enabled |= (1UL<<num);
}

void
yasm_warn_disable(yasm_warn_class num)
{
    warn_class_enabled &= ~(1UL<<num);
}

void
yasm_warn_disable_all(void)
{
    warn_class_enabled = 0;
}

unsigned int
yasm_get_num_errors(int warning_as_error)
{
    if (warning_as_error)
	return error_count+warning_count;
    else
	return error_count;
}

void
yasm_errwarn_output_all(yasm_linemap *lm, int warning_as_error,
     yasm_print_error_func print_error, yasm_print_warning_func print_warning)
{
    errwarn_data *we;
    const char *filename;
    unsigned long line;

    /* If we're treating warnings as errors, tell the user about it. */
    if (warning_as_error && warning_as_error != 2) {
	print_error("", 0,
		    yasm_gettext_hook(N_("warnings being treated as errors")));
	warning_as_error = 2;
    }

    /* Output error/warnings. */
    SLIST_FOREACH(we, &errwarns, link) {
	/* Output error/warning */
	yasm_linemap_lookup(lm, we->line, &filename, &line);
	if (we->type == WE_ERROR)
	    print_error(filename, line, we->msg);
	else
	    print_warning(filename, line, we->msg);
    }
}
