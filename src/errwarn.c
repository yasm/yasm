/* $IdPath$
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "util.h"

#include <stdio.h>
#include <ctype.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stdarg.h>
# include <string.h>
#endif

#include <libintl.h>
#define _(String)	gettext(String)
#ifdef gettext_noop
#define N_(String)	gettext_noop(String)
#else
#define N_(String)	(String)
#endif

#include "globals.h"
#include "errwarn.h"

RCSID("$IdPath$");

/* Total error count for entire assembler run.
 * Assembler should exit with EXIT_FAILURE if this is >= 0 on finish. */
static unsigned int error_count = 0;

/* Total warning count for entire assembler run.
 * Should not affect exit value of assembler. */
static unsigned int warning_count = 0;

/* See errwarn.h for constants that match up to these strings.
 * When adding a string here, keep errwarn.h in sync! */

/* Fatal error messages.  Match up with fatal_num enum in errwarn.h. */
static char *fatal_msgs[] = {
    N_("unknown"),
    N_("out of memory")
};

typedef STAILQ_HEAD(errwarnhead_s, errwarn_s) errwarnhead;
errwarnhead *errwarns = (errwarnhead *)NULL;

typedef struct errwarn_s {
    STAILQ_ENTRY(errwarn_s) link;

    enum { WE_ERROR, WE_WARNING } type;

    char *filename;
    unsigned long line;
    /* FIXME: This should not be a fixed size.  But we don't have vasprintf()
     * right now. */
    char msg[1024];
} errwarn;

/* Line number of the previous error.  Set and checked by Error(). */
static unsigned long previous_error_line = 0;

/* Is the last error a parser error?  Error() lets other errors override parser
 * errors. Set by yyerror(), read and reset by Error(). */
static int previous_error_parser = 0;

/* Line number of the previous warning. Set and checked by Warning(). */
static unsigned long previous_warning_line = 0;

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

/* Parser error handler.  Moves error into our error handling system. */
void
ParserError(char *s)
{
    Error("%s %s", _("parser error:"), s);
    previous_error_parser = 1;
}

/* Report an internal error.  Essentially a fatal error with trace info.
 * Exit immediately because it's essentially an assert() trap. */
void
InternalError(unsigned int line, char *file, char *message)
{
    fprintf(stderr, _("INTERNAL ERROR at %s, line %d: %s\n"), file, line,
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

/* Register an error.  Uses argtypes as described above to specify the
 * argument types.  Does not print the error, only stores it for
 * OutputAllErrorWarning() to print. */
void
Error(char *fmt, ...)
{
    va_list ap;
    errwarn *we;

    if ((previous_error_line == line_number) && !previous_error_parser)
	return;

    if (!errwarns) {
	errwarns = malloc(sizeof(errwarnhead));
	if (!errwarns)
	    Fatal(FATAL_NOMEM);
	STAILQ_INIT(errwarns);
    }

    if (previous_error_parser) {
	/* overwrite last (parser) error */	
	we = STAILQ_LAST(errwarns, errwarn_s, link);
    } else {
	we = malloc(sizeof(errwarn));
	if (!we)
	    Fatal(FATAL_NOMEM);

	we->type = WE_ERROR;
	we->filename = strdup(filename);
	if (!we->filename)
	    Fatal(FATAL_NOMEM);
	we->line = line_number;
    }

    va_start(ap, fmt);
    vsprintf(we->msg, fmt, ap);
    va_end(ap);

    if (!previous_error_parser)
	STAILQ_INSERT_TAIL(errwarns, we, link);

    previous_error_line = line_number;
    previous_error_parser = 0;

    error_count++;
}

/* Register a warning.  Uses argtypes as described above to specify the
 * argument types.  Does not print the warning, only stores it for
 * OutputAllErrorWarning() to print. */
void
Warning(char *fmt, ...)
{
    va_list ap;
    errwarn *we;

    if (previous_warning_line == line_number)
	return;

    previous_warning_line = line_number;

    we = malloc(sizeof(errwarn));
    if (!we)
	Fatal(FATAL_NOMEM);

    we->type = WE_WARNING;
    we->filename = strdup(filename);
    if (!we->filename)
	Fatal(FATAL_NOMEM);
    we->line = line_number;
    va_start(ap, fmt);
    vsprintf(we->msg, fmt, ap);
    va_end(ap);

    if (!errwarns) {
	errwarns = malloc(sizeof(errwarnhead));
	if (!errwarns)
	    Fatal(FATAL_NOMEM);
	STAILQ_INIT(errwarns);
    }
    STAILQ_INSERT_TAIL(errwarns, we, link);
    warning_count++;
}

void
ErrorNow(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

void
WarningNow(char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s ", _("warning:"));
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

void
ErrorAt(char *filename, unsigned long line, char *fmt, ...)
{
    /* TODO */
}

void
WarningAt(char *filename, unsigned long line, char *fmt, ...)
{
    /* TODO */
}

/* Output all previously stored errors and warnings to stderr. */
unsigned int
OutputAllErrorWarning(void)
{
    errwarn *we, *we2;

    /* If errwarns hasn't been initialized, there are no messages. */
    if (!errwarns)
	return error_count;

    /* Output error and warning messages. */
    STAILQ_FOREACH(we, errwarns, link) {
	if (we->type == WE_ERROR)
	    fprintf(stderr, "%s:%lu: %s\n", we->filename, we->line, we->msg);
	else
	    fprintf(stderr, "%s:%lu: %s %s\n", we->filename, we->line,
		    _("warning:"), we->msg);
    }

    /* Delete messages. */
    we = STAILQ_FIRST(errwarns);
    while (we) {
	we2 = STAILQ_NEXT(we, link);
	free(we);
	we = we2;
    }

    /* Return the total error count up to this point. */
    return error_count;
}
