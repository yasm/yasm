/*
 * Error and warning reporting and related functions.
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 *  This file is part of YASM.
 *
 *  YASM is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  YASM is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include <ctype.h>

#ifdef STDC_HEADERS
# include <stdarg.h>
#endif

#include "globals.h"
#include "errwarn.h"


#define MSG_MAXSIZE	1024

/* ALL warnings are disabled if this is nonzero. */
int warnings_disabled = 0;

/* Warnings are treated as errors if this is nonzero.
 * =2 indicates that warnings are treated as errors, and the message to the
 * user saying that has been output.
 */
int warning_error = 0;

/* Default enabled warnings.  See errwarn.h for a list. */
unsigned long warning_flags =
    (1UL<<WARN_UNRECOGNIZED_CHAR);

/* Total error count for entire assembler run.
 * Assembler should exit with EXIT_FAILURE if this is >= 0 on finish. */
static unsigned int error_count = 0;

/* Total warning count for entire assembler run.
 * Should not affect exit value of assembler. */
static unsigned int warning_count = 0;

/* See errwarn.h for constants that match up to these strings.
 * When adding a string here, keep errwarn.h in sync! */

/* Fatal error messages.  Match up with fatal_num enum in errwarn.h. */
/*@-observertrans@*/
static const char *fatal_msgs[] = {
    N_("unknown"),
    N_("out of memory")
};
/*@=observertrans@*/

typedef /*@reldef@*/ SLIST_HEAD(errwarnhead_s, errwarn_s) errwarnhead;
static /*@only@*/ /*@null@*/ errwarnhead errwarns =
    SLIST_HEAD_INITIALIZER(errwarns);

typedef struct errwarn_s {
    /*@reldef@*/ SLIST_ENTRY(errwarn_s) link;

    enum { WE_UNKNOWN, WE_ERROR, WE_WARNING, WE_PARSERERROR } type;

    unsigned long line;
    /* FIXME: This should not be a fixed size.  But we don't have vasprintf()
     * right now. */
    char msg[MSG_MAXSIZE];
} errwarn;

/* Last inserted error/warning.  Used to speed up insertions. */
static /*@null@*/ errwarn *previous_we = NULL;

/* Static buffer for use by conv_unprint(). */
static char unprint[5];

/* Convert a possibly unprintable character into a printable string, using
 * standard cat(1) convention for unprintable characters. */
char *
conv_unprint(char ch)
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
 * Exit immediately because it's essentially an assert() trap. */
void
InternalError_(const char *file, unsigned int line, const char *message)
{
    fprintf(stderr, _("INTERNAL ERROR at %s, line %u: %s\n"), file, line,
	    message);
#ifdef HAVE_ABORT
    abort();
#else
    exit(EXIT_FAILURE);
#endif
}

/* Report a fatal error.  These are unrecoverable (such as running out of
 * memory), so just exit immediately. */
void
Fatal(fatal_num num)
{
    fprintf(stderr, "%s %s\n", _("FATAL:"), gettext(fatal_msgs[num]));
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
static errwarn *
errwarn_new(unsigned long lindex, int replace_parser_error)
{
    errwarn *first, *next, *ins_we, *we;
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
    }

    if (replace_parser_error && ins_we && ins_we->type == WE_PARSERERROR) {
	/* overwrite last error */	
	we = ins_we;
    } else {
	/* add a new error */
	we = xmalloc(sizeof(errwarn));

	we->type = WE_UNKNOWN;
	we->line = lindex;

	if (action == INS_HEAD)
	    SLIST_INSERT_HEAD(&errwarns, we, link);
	else if (action == INS_AFTER) {
	    assert(ins_we != NULL);
	    SLIST_INSERT_AFTER(ins_we, we, link);
	} else
	    InternalError(_("Unexpected errwarn insert action"));
    }

    /* Remember previous err/warn */
    previous_we = we;

    return we;
}

/* Register an error.  Does not print the error, only stores it for
 * OutputAllErrorWarning() to print.
 */
static void
error_common(unsigned long lindex, const char *fmt, va_list ap)
{
    errwarn *we = errwarn_new(lindex, 1);

    we->type = WE_ERROR;

#ifdef HAVE_VSNPRINTF
    vsnprintf(we->msg, MSG_MAXSIZE, fmt, ap);
#else
    vsprintf(we->msg, fmt, ap);
#endif

    error_count++;
}

/* Register an warning.  Does not print the warning, only stores it for
 * OutputAllErrorWarning() to print.
 */
static void
warning_common(unsigned long lindex, const char *fmt, va_list va)
{
    errwarn *we = errwarn_new(lindex, 0);

    we->type = WE_WARNING;

#ifdef HAVE_VSNPRINTF
    vsnprintf(we->msg, MSG_MAXSIZE, fmt, va);
#else
    vsprintf(we->msg, fmt, va);
#endif

    warning_count++;
}

/* Parser error handler.  Moves YACC-style error into our error handling
 * system.
 */
void
ParserError(const char *s)
{
    Error("%s %s", _("parser error:"), s);
    previous_we->type = WE_PARSERERROR;
}

/* Register an error during the parser stage (uses global line_index).  Does
 * not print the error, only stores it for OutputAllErrorWarning() to print.
 */
void
Error(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    error_common(line_index, fmt, va);
    va_end(va);
}

/* Register an error at line lindex.  Does not print the error, only stores it
 * for OutputAllErrorWarning() to print.
 */
void
ErrorAt(unsigned long lindex, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    error_common(lindex, fmt, va);
    va_end(va);
}

/* Register a warning during the parser stage (uses global line_index).  Does
 * not print the warning, only stores it for OutputAllErrorWarning() to print.
 */
void
Warning(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    warning_common(line_index, fmt, va);
    va_end(va);
}

/* Register an warning at line lindex.  Does not print the warning, only stores
 * it for OutputAllErrorWarning() to print.
 */
void
WarningAt(unsigned long lindex, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    warning_common(lindex, fmt, va);
    va_end(va);
}

void
ErrorNow(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

void
WarningNow(const char *fmt, ...)
{
    va_list ap;

    if (warnings_disabled)
	return;

    fprintf(stderr, "%s ", _("warning:"));
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

/* Get the number of errors (including warnings if warnings are being treated
 * as errors).
 */
unsigned int
GetNumErrors(void)
{
    if (warning_error)
	return error_count+warning_count;
    else
	return error_count;
}

/* Output all previously stored errors and warnings to stderr. */
void
OutputAllErrorWarning(void)
{
    errwarn *we;
    const char *filename;
    unsigned long line;

    /* If we're treating warnings as errors, tell the user about it. */
    if (warning_error && warning_error != 2) {
	fprintf(stderr, "%s\n", _("warnings being treated as errors"));
	warning_error = 2;
    }

    /* Output error/warning, then delete each message. */
    while (!SLIST_EMPTY(&errwarns)) {
	we = SLIST_FIRST(&errwarns);
	/* Output error/warning */
	line_lookup(we->line, &filename, &line);
	if (we->type == WE_ERROR)
	    fprintf(stderr, "%s:%lu: %s\n", filename, line, we->msg);
	else
	    fprintf(stderr, "%s:%lu: %s %s\n", filename, line, _("warning:"),
		    we->msg);

	/* Delete */
	SLIST_REMOVE_HEAD(&errwarns, link);
	xfree(we);
    }
}
