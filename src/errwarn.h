/* $Id: errwarn.h,v 1.14 2001/08/30 03:45:26 peter Exp $
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

/* Fatal error constants.
 * See fatal_msgs in errwarn.c for strings that match up to these constants.
 * When adding a constant here, keep errwarn.c in sync! */
typedef enum {
    FATAL_UNKNOWN = 0,
    FATAL_NOMEM
} fatal_num;

char *conv_unprint(char ch);

void ParserError(char *);

void InternalError(unsigned int line, char *file, char *message);

void Fatal(fatal_num);
void Error(char *, ...);
void Warning(char *, ...);

void OutputError(void);
void OutputWarning(void);

#endif
