/* $Id: preproc.c,v 1.7 2001/09/15 07:16:59 peter Exp $
 * Raw preprocessor (preforms NO preprocessing)
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

#include <libintl.h>
#define _(String)	gettext(String)

#include "errwarn.h"

#include "objfmt.h"
#include "preproc.h"

RCSID("$Id: preproc.c,v 1.7 2001/09/15 07:16:59 peter Exp $");

static int is_interactive;
static FILE *in;

int isatty(int);

static void
initialize(objfmt *of, FILE *f)
{
    in = f;
    is_interactive = f ? (isatty(fileno(f)) > 0) : 0;
}

static int
input(char *buf, int max_size)
{
    int c = '*', n;

    if (is_interactive) {
	for (n = 0; n < max_size && (c = getc(in)) != EOF && c != '\n'; n++)
	    buf[n] = (char)c;
	if (c == '\n')
	    buf[n++] = (char)c;
	if (c == EOF && ferror(in))
	    Error(_("error when reading from file"));
    } else if (((n = fread(buf, 1, max_size, in)) == 0) && ferror(in))
	Error(_("error when reading from file"));

    return n;
}

/* Define preproc structure -- see preproc.h for details */
preproc raw_preproc = {
    "Disable preprocessing",
    "raw",
    initialize,
    input
};
