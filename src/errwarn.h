/* $IdPath$
 * Error and warning reporting and related functions header file.
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
#ifndef YASM_ERRWARN_H
#define YASM_ERRWARN_H

/* ALL warnings disabled? */
extern int warnings_disabled;

/* Warnings treated as errors? */
extern int warning_error;

/* Warning flags.  Currently a maximum of 32 are allowed.  This can be
 * increased by making this an array and modifying the WARN_ macros to use the
 * proper array index based on the warning number.
 *
 * If a bit is 1, it means that particular warning is enabled.  The bit numbers
 * are assigned according to the warn_flag_num enum.
 *
 * See errwarn.c for what warnings are enabled by default.
 */
extern unsigned long warning_flags;
typedef enum {
    WARN_UNRECOGNIZED_CHAR = 0
} warn_flag_num;

/* Tests the warning flags to see if warning "warn" is enabled */
#define WARN_ENABLED(warn)	(warning_flags & (1<<(warn)))
/* Sets warning "warn" to be enabled or disabled based on "s" (1=en, 0=dis). */
#define WARN_ENABLE(warn, s)	do { \
	warning_flags &= ~(1<<(warn)); \
	warning_flags |= (s)<<(warn); \
    } while(0)

/* Fatal error constants.
 * See fatal_msgs in errwarn.c for strings that match up to these constants.
 * When adding a constant here, keep errwarn.c in sync! */
typedef enum {
    FATAL_UNKNOWN = 0,
    FATAL_NOMEM
} fatal_num;

/*@shared@*/ char *conv_unprint(char ch);

void ParserError(unsigned long lindex, const char *);

/*@exits@*/ void InternalError_(const char *file, unsigned int line,
				const char *message);
#define InternalError(msg)	InternalError_(__FILE__, __LINE__, msg)

/*@exits@*/ void Fatal(fatal_num);

void Error(unsigned long lindex, const char *, ...) /*@printflike@*/;
void Warning(unsigned long lindex, const char *, ...) /*@printflike@*/;

/* These two functions immediately output the error or warning, with no file
 * or line information.  They should be used for errors and warnings outside
 * the parser stage (at program startup, for instance).
 */
void ErrorNow(const char *, ...) /*@printflike@*/;
void WarningNow(const char *, ...) /*@printflike@*/;

/* Returns total number of errors to this point in assembly. */
unsigned int GetNumErrors(void);

/* Outputs all errors/warnings to standard error. */
void OutputAllErrorWarning(linemgr *lm);

#endif
