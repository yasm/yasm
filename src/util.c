/* $Id: util.c,v 1.1 2001/06/13 05:24:50 peter Exp $
 * Various utility functions for ANSI C compatibility
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
#include <stdlib.h>
#include <string.h>
#include "util.h"

char *y_strdup(char *str)
{
    size_t len;
    char *copy;

    len = strlen(str) + 1;
    if((copy = malloc(len)) == NULL)
	return (char *)NULL;
    memcpy(copy, str, len);
    return copy;
}
