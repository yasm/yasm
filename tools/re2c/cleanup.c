/* $IdPath$
 *
 * Clean up re2c output to avoid compiler warnings.
 *
 *  Copyright (C) 2004  Peter Johnson
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINE	1024

int
main()
{
    char str[MAXLINE];
    size_t alloclines = 1000;
    size_t numlines = 0;
    char **inlines;
    size_t alloclabel = 100;
    unsigned char *usedlabel;
    size_t allocvar = 1000;
    unsigned char *usedvar;
    int lastusedvarline = 0;
    int level = 0;
    size_t line;
    unsigned long value;
    char *pos;
    size_t span1, span2;

    inlines = malloc(alloclines * sizeof(char *));
    if (!inlines) {
	fputs("Out of memory.\n", stderr);
	return EXIT_FAILURE;
    }

    while (fgets(str, MAXLINE, stdin)) {
	/* check array bounds */
	if (numlines >= alloclines) {
	    alloclines *= 2;
	    inlines = realloc(inlines, alloclines * sizeof(char *));
	    if (!inlines) {
		fputs("Out of memory.\n", stderr);
		return EXIT_FAILURE;
	    }
	}
	inlines[numlines] = strdup(str);
	numlines++;
    }

    usedlabel = calloc(alloclabel, 1);
    usedvar = calloc(allocvar, 1);
    if (!usedlabel || !usedvar) {
	fputs("Out of memory.\n", stderr);
	return EXIT_FAILURE;
    }

    for (line = 1; line <= numlines; line++) {
	/* look for goto yy[0-9]+ statements */
	if ((pos = strstr(inlines[line-1], "goto")) &&
	    (span1 = strspn(&pos[4], " \t")) > 0 &&
	    strncmp(&pos[4+span1], "yy", 2) == 0 &&
	    (span2 = strspn(&pos[6+span1], "0123456789")) > 0 &&
	    strspn(&pos[6+span1+span2], " \t;") > 0) {
	    /* convert label to integer */
	    value = strtoul(&pos[6+span1], NULL, 10);
	    /* check array bounds */
	    while (value >= alloclabel) {
		usedlabel = realloc(usedlabel, alloclabel * 2);
		if (!usedlabel) {
		    fputs("Out of memory.\n", stderr);
		    return EXIT_FAILURE;
		}
		memset(usedlabel + alloclabel, 0, alloclabel);
		alloclabel *= 2;
	    }
	    usedlabel[value] = 1;
	}

	/* keep track of the brace level of the code (approximately) */
	pos = inlines[line-1];
	while ((pos = strchr(pos, '{'))) {
	    level++;
	    pos++;
	}
	pos = inlines[line-1];
	while ((pos = strchr(pos, '}'))) {
	    level--;
	    pos++;
	}

	/* check for leaving the scope of the last used yyaccept variable */
	if (level < usedvar[lastusedvarline])
	    lastusedvarline = 0;

	/* check for int yyaccept variable declaration / usage */
	if ((pos = strstr(inlines[line-1], "int")) &&
	    (span1 = strspn(&pos[3], " \t")) > 0 &&
	    strncmp(&pos[3+span1], "yyaccept", 8) == 0 &&
	    strspn(&pos[11+span1], " \t;") > 0) {
	    /* declaration */
	    /* check array bounds */
	    while (line >= allocvar) {
		usedvar = realloc(usedvar, allocvar * 2);
		if (!usedvar) {
		    fputs("Out of memory.\n", stderr);
		    return EXIT_FAILURE;
		}
		memset(usedvar + allocvar, 0, allocvar);
		allocvar *= 2;
	    }
	    usedvar[line] = level;
	    lastusedvarline = line;
	} else if (strstr(inlines[line-1], "yyaccept"))
	    usedvar[lastusedvarline] = 255;	    /* used */
    }

    for (line = 1; line <= numlines; line++) {
	pos = inlines[line-1];
	/* look for yy[0-9]+ labels */
	if (strncmp(pos, "yy", 2) == 0 &&
	    (span1 = strspn(&pos[2], "0123456789")) > 0 &&
	    pos[2+span1] == ':') {
	    value = strtoul(&pos[2], NULL, 10);
	    /* delete unused yy[0-9]+ labels */
	    if (value >= alloclabel || !usedlabel[value])
		pos = &pos[2+span1+1];
	}
	if (line < allocvar && usedvar[line] != 0 && usedvar[line] != 255)
	    putc('\n', stdout);
	else
	    fputs(pos, stdout);
    }

    free(usedvar);
    free(usedlabel);
    for (line = 0; line < numlines; line++)
	free(inlines[line]);
    free(inlines);

    return EXIT_SUCCESS;
}
