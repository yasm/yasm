/* $Id: errwarn.c,v 1.22 2001/08/19 07:46:52 peter Exp $
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

#include "globals.h"
#include "errwarn.h"

RCSID("$Id: errwarn.c,v 1.22 2001/08/19 07:46:52 peter Exp $");

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
    "unknown",
    "out of memory"
};

/* Error messages.  Match up with err_num enum in errwarn.h. */
static char *err_msgs[] = {
    "",
    "parser error: %s",
    "missing '%1'",
    "missing argument to %s",
    "invalid argument to %s",
    "invalid effective address",
    "label or instruction expected at start of line",
    "expression syntax error",
    "duplicate definition of `%1'; previously defined line %2",
    "mismatch in operand sizes",
    "no %s form of that jump instruction exists",
    "unterminated string",
    "unexpected end of file in string",
    "expression syntax error",
    "floating-point constant encountered in `%s'",
    "non-floating-point value encountered in `%s'",
    "could not open file `%s'",
    "error when reading from file"
};

/* Warning messages.  Match up with warn_num enum in errwarn.h. */
static char *warn_msgs[] = {
    "",
    "ignoring unrecognized character '%s'",
    "%s value exceeds bounds",
    "multiple segment overrides, using leftmost",
    "multiple LOCK or REP prefixes, using leftmost",
    "no non-local label before '%s'",
    "multiple SHORT or NEAR specifiers, using leftmost",
    "character constant too large, ignoring trailing characters"
};

/* I hate to define these strings as static buffers; a better solution would be
 * to use vasprintf() to dynamically allocate, but that's not ANSI C.
 * FIXME! */

/* Last error message string.  Set by Error(), read by OutputError(). */
static char last_err[1024];

/* Last warning message string.  Set by Warning(), read by OutputWarning(). */
static char last_warn[1024];

/* Last error number.  Set by Error(), read and reset by OutputError(). */
static err_num last_err_num = ERR_NONE;

/* Last warning number.  Set by Warning(), read and reset by
 * OutputWarning(). */
static warn_num last_warn_num = WARN_NONE;

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
yyerror(char *s)
{
    Error(ERR_PARSER, (char *)NULL, s);
}

/* Report an internal error.  Essentially a fatal error with trace info.
 * Exit immediately because it's essentially an assert() trap. */
void
InternalError(unsigned int line, char *file, char *message)
{
    fprintf(stderr, "INTERNAL ERROR at %s, line %d: %s\n", file, line,
	    message);
    exit(EXIT_FAILURE);
}

/* Report a fatal error.  These are unrecoverable (such as running out of
 * memory), so just exit immediately. */
void
Fatal(fatal_num num)
{
    fprintf(stderr, "FATAL: %s\n", fatal_msgs[num]);
    exit(EXIT_FAILURE);
}

/* Argument replacement function for use in error messages.
 * Replaces %1, %2, etc in src with %c, %s, etc. in argtypes.
 * Currently limits maximum number of args to 9 (%1-%9).
 *
 * We need this because messages that take multiple arguments become dependent
 * on the order and type of the arguments passed to Error()/Warning().
 *
 * i.e. an error string "'%d' is not a valid specifier for '%s'" would require
 * that the arguments passed to Error() always be an int and a char *, in that
 * order.  If the string was changed to be "'%s': invalid specifier '%d'", all
 * the times Error() was called for that string would need to be changed to
 * reorder the arguments.  Or if the %d was not right in some circumstances,
 * we'd have to add another string for that type.
 *
 * This function fixes this problem by allowing the string to be specified as
 * "'%1' is not a valid specifier for '%2'" and then specifying at the time of
 * the Error() call what the types of %1 and %2 are by passing a argtype string
 * "%d%s" (to emulate the first behavior).  If the string was changed to be
 * "'%2': invalid specifier '%1'", no change would need to be made to the
 * Error calls using that string.  And as the type is specified with the
 * argument list, mismatches are far less likely.
 *
 * For strings that only have one argument of a fixed type, it can be directly
 * specified and NULL passed for the argtypes parameter when Error() is
 * called. */
static char *
process_argtypes(char *src, char *argtypes)
{
    char *dest;
    char *argtype[9];
    int at_num;
    char *destp, *srcp, *argtypep;

    if (argtypes) {
	dest = malloc(strlen(src) + strlen(argtypes));
	if (!dest)
	    Fatal(FATAL_NOMEM);
	/* split argtypes by % */
	at_num = 0;
	while ((argtypes = strchr(argtypes, '%')) && at_num < 9)
	    argtype[at_num++] = ++argtypes;
	/* search through src for %, copying as we go */
	destp = dest;
	srcp = src;
	while (*srcp != '\0') {
	    *(destp++) = *srcp;
	    if (*(srcp++) == '%') {
		if (isdigit(*srcp)) {
		    /* %1, %2, etc */
		    argtypep = argtype[*srcp - '1'];
		    while ((*argtypep != '%') && (*argtypep != '\0'))
			*(destp++) = *(argtypep++);
		} else
		    *(destp++) = *srcp;
		srcp++;
	    }
	}
    } else {
	dest = strdup(src);
	if (!dest)
	    Fatal(FATAL_NOMEM);
    }
    return dest;
}

/* Register an error.  Uses argtypes as described above to specify the
 * argument types.  Does not print the error, only stores it for
 * OutputError() to print. */
void
Error(err_num num, char *argtypes, ...)
{
    va_list ap;
    char *printf_str;

    if ((last_err_num != ERR_NONE) && (last_err_num != ERR_PARSER))
	return;

    last_err_num = num;

    printf_str = process_argtypes(err_msgs[num], argtypes);

    va_start(ap, argtypes);
    vsprintf(last_err, printf_str, ap);
    va_end(ap);

    free(printf_str);

    error_count++;
}

/* Register a warning.  Uses argtypes as described above to specify the
 * argument types.  Does not print the warning, only stores it for
 * OutputWarning() to print. */
void
Warning(warn_num num, char *argtypes, ...)
{
    va_list ap;
    char *printf_str;

    if (last_warn_num != WARN_NONE)
	return;

    last_warn_num = num;

    printf_str = process_argtypes(warn_msgs[num], argtypes);

    va_start(ap, argtypes);
    vsprintf(last_warn, printf_str, ap);
    va_end(ap);

    free(printf_str);

    warning_count++;
}

/* Output a previously stored error (if any) to stderr. */
void
OutputError(void)
{
    if (last_err_num != ERR_NONE)
	fprintf(stderr, "%s:%u: %s\n", filename, line_number, last_err);
    last_err_num = ERR_NONE;
}

/* Output a previously stored warning (if any) to stderr. */
void
OutputWarning(void)
{
    if (last_warn_num != WARN_NONE)
	fprintf(stderr, "%s:%u: warning: %s\n", filename, line_number,
		last_warn);
    last_warn_num = WARN_NONE;
}
