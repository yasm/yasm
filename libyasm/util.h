/* $Id: util.h,v 1.5 2001/08/19 04:32:02 peter Exp $
 * Defines prototypes for replacement functions if needed.
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
#ifndef YASM_UTIL_H
#define YASM_UTIL_H

#ifndef HAVE_STRDUP
char *strdup(const char *str);
#endif

#ifndef HAVE_STRTOUL
unsigned long strtoul(const char *nptr, char **endptr, int base);
#endif

#ifndef HAVE_TOASCII
# define toascii(c) ((c) & 0x7F)
#endif

#if defined(HAVE_SYS_QUEUE_H) && !defined(HAVE_BOGUS_SYS_QUEUE_H)
#include <sys/queue.h>
#else
#include "compat-queue.h"
#endif

#endif
