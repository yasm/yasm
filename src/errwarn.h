/* $Id: errwarn.h,v 1.12 2001/08/18 22:15:12 peter Exp $
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

/* See errwarn.c for strings that match up to these constants.
 * When adding a constant here, keep errwarn.c in sync! */

/* Fatal error constants.  Match up with fatal_msgs in errwarn.c. */
typedef enum {
    FATAL_UNKNOWN = 0,
    FATAL_NOMEM
} fatal_num;

/* Error constants.  Match up with err_msgs in errwarn.c. */
/* FIXME: These shouldn't be ERR_* because they'll violate namespace
 * constraints if errno.h is included. */
typedef enum {
    ERR_NONE = 0,
    ERR_PARSER,
    ERR_MISSING,
    ERR_MISSING_ARG,
    ERR_INVALID_ARG,
    ERR_INVALID_EA,
    ERR_INVALID_LINE,
    ERR_EXP_SYNTAX,
    ERR_DUPLICATE_DEF,
    ERR_OP_SIZE_MISMATCH,
    ERR_NO_JMPREL_FORM,
    ERR_STRING_UNTERM,
    ERR_STRING_EOF,
    ERR_EXPR_SYNTAX,
    ERR_DECLDATA_FLOAT,
    ERR_DECLDATA_EXPR,
    ERR_FILE_OPEN
} err_num;

/* Warning constants.  Match up with warn_msgs in errwarn.c. */
typedef enum {
    WARN_NONE = 0,
    WARN_UNREC_CHAR,
    WARN_VALUE_EXCEEDS_BOUNDS,
    WARN_MULT_SEG_OVERRIDE,
    WARN_MULT_LOCKREP_PREFIX,
    WARN_NO_BASE_LABEL,
    WARN_MULT_SHORTNEAR,
    WARN_CHAR_CONST_TOO_BIG
} warn_num;

char *conv_unprint(char ch);

void InternalError(unsigned int line, char *file, char *message);

void Fatal(fatal_num);
void Error(err_num, char *, ...);
void Warning(warn_num, char *, ...);

void OutputError(void);
void OutputWarning(void);

#endif
