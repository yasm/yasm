#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "error.h"

/*
  Check: a unit test framework for C
  Copyright (C) 2001, Arien Malec

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

void eprintf (char *fmt, ...)
{
  va_list args;
  fflush(stdout);

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  /*include system error information if format ends in colon */
  if (fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':')
    fprintf(stderr, " %s", strerror(errno));
  fprintf(stderr, "\n");

  exit(2);
}

void *emalloc (size_t n)
{
  void *p;
  p = malloc(n);
  if (p == NULL)
    eprintf("malloc of %u bytes failed:", n);
  return p;
}

void *erealloc (void * ptr, size_t n)
{
  void *p;
  p = realloc (ptr, n);
  if (p == NULL)
    eprintf("realloc of %u bytes failed:", n);
  return p;
}
