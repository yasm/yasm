/* $Id: errwarn.c,v 1.23 2001/08/30 03:45:26 peter Exp $
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

RCSID("$Id: errwarn.c,v 1.23 2001/08/30 03:45:26 peter Exp $");

/* Total error count for entire assembler run.
 * Assembler should exit with EXIT_FAILURE if this is >= 0 on finish. */
unsigned int error_count = 0;

/* Total warning count for entire assembler run.
 * Should not affect exit value of assembler. */
unsigned int warning_count = 0;

/* See errwarn.h for constants that match up to these strings.
 * When adding a string here, keep errwarn.h in sync! */

/* Fatal error messages.  Match up with fatal_num enum in errwarn.h. */
static char *fatal_msgs[] = {
    N_("unknown"),
    N_("out of memory")
};

/* I hate to define these strings as static buffers; a better solution would be
 * to use vasprintf() to dynamically allocate, but that's not ANSI C.
 * FIXME! */

/* Last error message string.  Set by Error(), read by OutputError(). */
static char last_err[1024];

/* Last warning message string.  Set by Warning(), read by OutputWarning(). */
static char last_warn[1024];

/* Has there been an error since the last time we output one?  Set by Error(),
 * read and reset by OutputError(). */
static int new_error = 0;

/* Is the last error a parser error?  Error() lets other errors override parser
 * errors. Set by yyerror(), read and reset by Error(). */
static int parser_error = 0;

/* Has there been a warning since the last time we output one?  Set by
 * Warning(), read and reset by OutputWarning(). */
static int new_warning = 0;

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
    parser_error = 1;
}

/* Report an internal error.  Essentially a fatal error with trace info.
 * Exit immediately because it's essentially an assert() trap. */
void
InternalError(unsigned int line, char *file, char *message)
{
    fprintf(stderr, _("INTERNAL ERROR at %s, line %d: %s\n"), file, line,
	    message);
    exit(EXIT_FAILURE);
}

/* Report a fatal error.  These are unrecoverable (such as running out of
 * memory), so just exit immediately. */
void
Fatal(fatal_num num)
{
    fprintf(stderr, "%s %s\n", _("FATAL:"), gettext(fatal_msgs[num]));
    exit(EXIT_FAILURE);
}

/* Register an error.  Uses argtypes as described above to specify the
 * argument types.  Does not print the error, only stores it for
 * OutputError() to print. */
void
Error(char *fmt, ...)
{
    va_list ap;

    if (new_error && !parser_error)
	return;

    new_error = 1;
    parser_error = 0;

    va_start(ap, fmt);
    vsprintf(last_err, fmt, ap);
    va_end(ap);

    error_count++;
}

/* Register a warning.  Uses argtypes as described above to specify the
 * argument types.  Does not print the warning, only stores it for
 * OutputWarning() to print. */
void
Warning(char *fmt, ...)
{
    va_list ap;

    if (new_warning)
	return;

    new_warning = 1;

    va_start(ap, fmt);
    vsprintf(last_warn, fmt, ap);
    va_end(ap);

    warning_count++;
}

/* Output a previously stored error (if any) to stderr. */
void
OutputError(void)
{
    if (new_error)
	fprintf(stderr, "%s:%u: %s\n", filename, line_number, last_err);
    new_error = 0;
}

/* Output a previously stored warning (if any) to stderr. */
void
OutputWarning(void)
{
    if (new_warning)
	fprintf(stderr, "%s:%u: %s %s\n", filename, line_number,
		_("warning:"), last_warn);
    new_warning = 0;
}
