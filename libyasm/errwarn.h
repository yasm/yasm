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

/* Warning classes (may be enabled/disabled). */
typedef enum {
    WARN_GENERAL = 0,	    /* non-specific warnings */
    WARN_UNREC_CHAR,	    /* unrecognized characters (while tokenizing) */
    WARN_PREPROC	    /* preprocessor warnings */
} warn_class_num;

/* Fatal error constants.
 * See fatal_msgs in errwarn.c for strings that match up to these constants.
 * When adding a constant here, keep errwarn.c in sync!
 */
typedef enum {
    FATAL_UNKNOWN = 0,
    FATAL_NOMEM
} fatal_num;

struct errwarn {
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
    /*@exits@*/ void (*fatal) (fatal_num);

    void (*error) (unsigned long lindex, const char *, ...) /*@printflike@*/;
    void (*warning) (warn_class_num, unsigned long lindex, const char *, ...)
	/*@printflike@*/;

    /* Logs a parser error.  These can be overwritten by non-parser errors on
     * the same line.
     */
    void (*parser_error) (unsigned long lindex, const char *);

    /* Enables/disables a class of warnings. */
    void (*warn_enable) (warn_class_num);
    void (*warn_disable) (warn_class_num);
    void (*warn_disable_all) (void);

    /* Returns total number of errors logged to this point.
     * If warning_as_error is nonzero, warnings are treated as errors.
     */
    unsigned int (*get_num_errors) (int warning_as_error);

    /* Outputs all errors/warnings (e.g. to stderr or elsewhere). */
    void (*output_all) (linemgr *lm, int warning_as_error);

    /* Convert a possibly unprintable character into a printable string. */
    char * (*conv_unprint) (char ch);
};

#endif
