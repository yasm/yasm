/* $Id$
 *
 * Generate version.mac
 *
 *  Copyright (C) 2006  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"	/* for PACKAGE_VERSION */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char *argv[])
{
    FILE *out;
    int major, minor, subminor;

    if (argc != 2) {
	fprintf(stderr, "Usage: %s <outfile>\n", argv[0]);
	return EXIT_FAILURE;
    }

    out = fopen(argv[1], "wt");

    if (!out) {
	fprintf(stderr, "Could not open `%s'.\n", argv[1]);
	return EXIT_FAILURE;
    }

    fprintf(out, "; This file auto-generated by genversion.c"
		 " - don't edit it\n");

    if (sscanf(PACKAGE_INTVER, "%d.%d.%d", &major, &minor, &subminor) != 3) {
	fprintf(stderr, "Version tokenizing error\n");
	fclose(out);
	remove(argv[1]);
	return EXIT_FAILURE;
    }

    fprintf(out, "%%define __YASM_MAJOR__ %d\n", major);
    fprintf(out, "%%define __YASM_MINOR__ %d\n", minor);
    fprintf(out, "%%define __YASM_SUBMINOR__ %d\n", subminor);
    if (!isdigit(PACKAGE_BUILD[0]))
	fprintf(out, "%%define __YASM_BUILD__ 0\n");
    else
	fprintf(out, "%%define __YASM_BUILD__ %d\n", atoi(PACKAGE_BUILD));

    /* Version id (hex number) */
    fprintf(out, "%%define __YASM_VERSION_ID__ 0%02x%02x%02x00h\n", major,
	    minor, subminor);

    /* Version string - version sans build */
    fprintf(out, "%%define __YASM_VER__ \"%s\"\n", PACKAGE_INTVER);
    fclose(out);

    return EXIT_SUCCESS;
}