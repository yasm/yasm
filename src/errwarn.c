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
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include <ctype.h>

#ifdef STDC_HEADERS
# include <stdarg.h>
#endif

#include "linemgr.h"
#include "errwarn.h"


#define MSG_MAXSIZE	1024

/* Enabled warnings.  See errwarn.h for a list. */
static unsigned long warn_class_enabled;

/* Total error count */
static unsigned int error_count;

/* Total warning count */
static unsigned int warning_count;

/* See errwarn.h for constants that match up to these strings.
 * When adding a string here, keep errwarn.h in sync!
 */

/* Fatal error messages.  Match up with fatal_num enum in errwarn.h. */
/*@-observertrans@*/
static const char *fatal_msgs[] = {
    N_("unknown"),
    N_("out of memory")
};
/*@=observertrans@*/

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


static void
yasm_errwarn_initialize(void)
{
    /* Default enabled warnings.  See errwarn.h for a list. */
    warn_class_enabled = 
	(1UL<<WARN_GENERAL) | (1UL<<WARN_UNREC_CHAR) | (1UL<<WARN_PREPROC);

    error_count = 0;
    warning_count = 0;
    SLIST_INIT(&errwarns);
    previous_we = NULL;
}

static void
yasm_errwarn_cleanup(void)
{
    errwarn_data *we;

    /* Delete all error/warnings */
    while (!SLIST_EMPTY(&errwarns)) {
	we = SLIST_FIRST(&errwarns);

	SLIST_REMOVE_HEAD(&errwarns, link);
	xfree(we);
    }
}

/* Convert a possibly unprintable character into a printable string, using
 * standard cat(1) convention for unprintable characters.
 */
static char *
yasm_errwarn_conv_unprint(char ch)
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
yasm_errwarn_internal_error_(const char *file, unsigned int line,
			     const char *message)
{
    fprintf(stderr, _("INTERNAL ERROR at %s, line %u: %s\n"), file, line,
	    gettext(message));
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
yasm_errwarn_fatal(fatal_num num)
{
    fprintf(stderr, _("FATAL: %s\n"), gettext(fatal_msgs[num]));
#ifdef HAVE_ABORT
    abort();
#else
    exit(EXIT_FAILURE);
#endif
}

/* Create an errwarn structure in the correct linked list location.
 * If replace_parser_error is nonzero, overwrites the last error if its
 * type is WE_PARSERERROR.
 */
static errwarn_data *
errwarn_data_new(unsigned long lindex, int replace_parser_error)
{
    errwarn_data *first, *next, *ins_we, *we;
    enum { INS_NONE, INS_HEAD, INS_AFTER } action = INS_NONE;

    /* Find the entry with either line=lindex or the last one with line<lindex.
     * Start with the last entry added to speed the search.
     */
    ins_we = previous_we;
    first = SLIST_FIRST(&errwarns);
    if (!ins_we || !first)
	action = INS_HEAD;
    while (action == INS_NONE) {
	next = SLIST_NEXT(ins_we, link);
	if (lindex < ins_we->line) {
	    if (ins_we == first)
		action = INS_HEAD;
	    else
		ins_we = first;
	} else if (!next)
	    action = INS_AFTER;
	else if (lindex >= ins_we->line && lindex < next->line)
	    action = INS_AFTER;
	else
	    ins_we = next;
    }

    if (replace_parser_error && ins_we && ins_we->type == WE_PARSERERROR) {
	/* overwrite last error */	
	we = ins_we;
    } else {
	/* add a new error */
	we = xmalloc(sizeof(errwarn_data));

	we->type = WE_UNKNOWN;
	we->line = lindex;

	if (action == INS_HEAD)
	    SLIST_INSERT_HEAD(&errwarns, we, link);
	else if (action == INS_AFTER) {
	    assert(ins_we != NULL);
	    SLIST_INSERT_AFTER(ins_we, we, link);
	} else
	    yasm_errwarn_internal_error_(__FILE__, __LINE__,
		N_("Unexpected errwarn insert action"));
    }

    /* Remember previous err/warn */
    previous_we = we;

    return we;
}

/* Register an error at line lindex.  Does not print the error, only stores it
 * for output_all() to print.
 */
static void
yasm_errwarn_error_va(unsigned long lindex, const char *fmt, va_list va)
{
    errwarn_data *we = errwarn_data_new(lindex, 1);

    we->type = WE_ERROR;

#ifdef HAVE_VSNPRINTF
    vsnprintf(we->msg, MSG_MAXSIZE, gettext(fmt), va);
#else
    vsprintf(we->msg, gettext(fmt), va);
#endif

    error_count++;
}

/* Register an warning at line lindex.  Does not print the warning, only stores
 * it for output_all() to print.
 */
static void
yasm_errwarn_warning_va(warn_class_num num, unsigned long lindex,
			const char *fmt, va_list va)
{
    errwarn_data *we;

    if (!(warn_class_enabled & (1UL<<num)))
	return;	    /* warning is part of disabled class */

    we = errwarn_data_new(lindex, 0);

    we->type = WE_WARNING;

#ifdef HAVE_VSNPRINTF
    vsnprintf(we->msg, MSG_MAXSIZE, gettext(fmt), va);
#else
    vsprintf(we->msg, gettext(fmt), va);
#endif

    warning_count++;
}

/* Register an error at line lindex.  Does not print the error, only stores it
 * for output_all() to print.
 */
static void
yasm_errwarn_error(unsigned long lindex, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    yasm_errwarn_error_va(lindex, fmt, va);
    va_end(va);
}

/* Register an warning at line lindex.  Does not print the warning, only stores
 * it for output_all() to print.
 */
static void
yasm_errwarn_warning(warn_class_num num, unsigned long lindex, const char *fmt,
		     ...)
{
    va_list va;
    va_start(va, fmt);
    yasm_errwarn_warning_va(num, lindex, fmt, va);
    va_end(va);
}

/* Parser error handler.  Moves YACC-style error into our error handling
 * system.
 */
static void
yasm_errwarn_parser_error(unsigned long lindex, const char *s)
{
    yasm_errwarn_error(lindex, N_("parser error: %s"), s);
    previous_we->type = WE_PARSERERROR;
}

static void
yasm_errwarn_warn_enable(warn_class_num num)
{
    warn_class_enabled |= (1UL<<num);
}

static void
yasm_errwarn_warn_disable(warn_class_num num)
{
    warn_class_enabled &= ~(1UL<<num);
}

static void
yasm_errwarn_warn_disable_all(void)
{
    warn_class_enabled = 0;
}

/* Get the number of errors (including warnings if warnings are being treated
 * as errors).
 */
static unsigned int
yasm_errwarn_get_num_errors(int warning_as_error)
{
    if (warning_as_error)
	return error_count+warning_count;
    else
	return error_count;
}

/* Output all previously stored errors and warnings to stderr. */
static void
yasm_errwarn_output_all(linemgr *lm, int warning_as_error)
{
    errwarn_data *we;
    const char *filename;
    unsigned long line;

    /* If we're treating warnings as errors, tell the user about it. */
    if (warning_as_error && warning_as_error != 2) {
	fprintf(stderr, "%s\n", _("warnings being treated as errors"));
	warning_as_error = 2;
    }

    /* Output error/warnings. */
    SLIST_FOREACH(we, &errwarns, link) {
	/* Output error/warning */
	lm->lookup(we->line, &filename, &line);
	if (we->type == WE_ERROR)
	    fprintf(stderr, "%s:%lu: %s\n", filename, line, we->msg);
	else
	    fprintf(stderr, "%s:%lu: %s %s\n", filename, line, _("warning:"),
		    we->msg);
    }
}

errwarn yasm_errwarn = {
    yasm_errwarn_initialize,
    yasm_errwarn_cleanup,
    yasm_errwarn_internal_error_,
    yasm_errwarn_fatal,
    yasm_errwarn_error_va,
    yasm_errwarn_warning_va,
    yasm_errwarn_error,
    yasm_errwarn_warning,
    yasm_errwarn_parser_error,
    yasm_errwarn_warn_enable,
    yasm_errwarn_warn_disable,
    yasm_errwarn_warn_disable_all,
    yasm_errwarn_get_num_errors,
    yasm_errwarn_output_all,
    yasm_errwarn_conv_unprint
};
