/*
 * Little-endian file functions.
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
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "file.h"

#include "errwarn.h"


char *
replace_extension(const char *orig, /*@null@*/ const char *ext,
		  const char *def)
{
    char *out, *outext;

    /* allocate enough space for full existing name + extension */
    out = xmalloc(strlen(orig)+(ext ? (strlen(ext)+2) : 1));
    strcpy(out, orig);
    outext = strrchr(out, '.');
    if (outext) {
	/* Existing extension: make sure it's not the same as the replacement
	 * (as we don't want to overwrite the source file).
	 */
	outext++;   /* advance past '.' */
	if (ext && strcmp(outext, ext) == 0) {
	    outext = NULL;  /* indicate default should be used */
	    WarningNow(_("file name already ends in `.%s': output will be in `%s'"),
		       ext, def);
	}
    } else {
	/* No extension: make sure the output extension is not empty
	 * (again, we don't want to overwrite the source file).
	 */
	if (!ext)
	    WarningNow(_("file name already has no extension: output will be in `%s'"),
		       def);
	else {
	    outext = strrchr(out, '\0');    /* point to end of the string */
	    *outext++ = '.';		    /* append '.' */
	}
    }

    /* replace extension or use default name */
    if (outext) {
	if (!ext) {
	    /* Back up and replace '.' with string terminator */
	    outext--;
	    *outext = '\0';
	} else
	    strcpy(outext, ext);
    } else
	strcpy(out, def);

    return out;
}

size_t
fwrite_short(unsigned short val, FILE *f)
{
    if (fputc(val & 0xFF, f) == EOF)
	return 0;
    if (fputc((val >> 8) & 0xFF, f) == EOF)
	return 0;
    return 1;
}

size_t
fwrite_long(unsigned long val, FILE *f)
{
    if (fputc((int)(val & 0xFF), f) == EOF)
	return 0;
    if (fputc((int)((val >> 8) & 0xFF), f) == EOF)
	return 0;
    if (fputc((int)((val >> 16) & 0xFF), f) == EOF)
	return 0;
    if (fputc((int)((val >> 24) & 0xFF), f) == EOF)
	return 0;
    return 1;
}
