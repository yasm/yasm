/* $Id: errwarn.h,v 1.3 2001/05/22 20:44:32 peter Exp $
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
#ifndef _ERRWARN_H_
#define _ERRWARN_H_

char *conv_unprint(char ch);

typedef enum {
    FATAL_UNKNOWN = 0,
    FATAL_NOMEM
} fatal_num;

void Fatal(fatal_num);

typedef enum {
    ERR_NONE = 0,
    ERR_PARSER,
    ERR_MISSING,
    ERR_MISSING_ARG,
    ERR_INVALID_ARG,
    ERR_INVALID_EA,
    ERR_INVALID_LINE,
    ERR_EXP_SYNTAX
} err_num;

void Error(err_num, char *, ...);

typedef enum {
    WARN_NONE = 0,
    WARN_UNREC_CHAR,
    WARN_VALUE_EXCEEDS_BOUNDS
} warn_num;

void Warning(warn_num, char *, ...);

void OutputError(void);
void OutputWarning(void);

#endif
