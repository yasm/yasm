/* nasmlib.c	library routines for the Netwide Assembler
 *
 * The Netwide Assembler is copyright (C) 1996 Simon Tatham and
 * Julian Hall. All rights reserved. The software is
 * redistributable under the licence given in the file "Licence"
 * distributed in the NASM archive.
 */
#define YASM_LIB_INTERNAL
#include "libyasm/util.h"
#include <ctype.h>

#include "nasm.h"
#include "nasmlib.h"
/*#include "insns.h"*/		/* For MAX_KEYWORD */

#define lib_isnumchar(c)   ( isalnum(c) || (c) == '$')
#define numvalue(c)  ((c)>='a' ? (c)-'a'+10 : (c)>='A' ? (c)-'A'+10 : (c)-'0')

long nasm_readnum (char *str, int *error) 
{
    char *r = str, *q;
    long radix;
    unsigned long result, checklimit;
    int digit, last;
    int warn = FALSE;
    int sign = 1;

    *error = FALSE;

    while (isspace(*r)) r++;	       /* find start of number */

    /*
     * If the number came from make_tok_num (as a result of an %assign), it
     * might have a '-' built into it (rather than in a preceeding token).
     */
    if (*r == '-')
    {
	r++;
	sign = -1;
    }

    q = r;

    while (lib_isnumchar(*q)) q++;     /* find end of number */

    /*
     * If it begins 0x, 0X or $, or ends in H, it's in hex. if it
     * ends in Q, it's octal. if it ends in B, it's binary.
     * Otherwise, it's ordinary decimal.
     */
    if (*r=='0' && (r[1]=='x' || r[1]=='X'))
	radix = 16, r += 2;
    else if (*r=='$')
	radix = 16, r++;
    else if (q[-1]=='H' || q[-1]=='h')
	radix = 16 , q--;
    else if (q[-1]=='Q' || q[-1]=='q')
	radix = 8 , q--;
    else if (q[-1]=='B' || q[-1]=='b')
	radix = 2 , q--;
    else
	radix = 10;

    /*
     * If this number has been found for us by something other than
     * the ordinary scanners, then it might be malformed by having
     * nothing between the prefix and the suffix. Check this case
     * now.
     */
    if (r >= q) {
	*error = TRUE;
	return 0;
    }

    /*
     * `checklimit' must be 2**32 / radix. We can't do that in
     * 32-bit arithmetic, which we're (probably) using, so we
     * cheat: since we know that all radices we use are even, we
     * can divide 2**31 by radix/2 instead.
     */
    checklimit = 0x80000000UL / (radix>>1);

    /*
     * Calculate the highest allowable value for the last digit
     * of a 32 bit constant... in radix 10, it is 6, otherwise it is 0
     */
    last = (radix == 10 ? 6 : 0);

    result = 0;
    while (*r && r < q) {
	if (*r<'0' || (*r>'9' && *r<'A') || (digit = numvalue(*r)) >= radix) 
	{
	    *error = TRUE;
	    return 0;
	}
	if (result > checklimit ||
	    (result == checklimit && digit >= last))
	{
	    warn = TRUE;
	}

	result = radix * result + digit;
	r++;
    }
#if 0
    if (warn)
	nasm_malloc_error (ERR_WARNING | ERR_PASS1 | ERR_WARN_NOV,
			   "numeric constant %s does not fit in 32 bits",
			   str);
#endif
    return result*sign;
}

long nasm_readstrnum (char *str, int length, int *warn) 
{
    long charconst = 0;
    int i;

    *warn = FALSE;

    str += length;
    for (i=0; i<length; i++) {
	if (charconst & 0xff000000UL) {
	    *warn = TRUE;
	}
	charconst = (charconst<<8) + (unsigned char) *--str;
    }
    return charconst;
}

static long next_seg;

void nasm_seg_init(void) 
{
    next_seg = 0;
}

long nasm_seg_alloc(void) 
{
    return (next_seg += 2) - 2;
}

/*
 * Return TRUE if the argument is a simple scalar. (Or a far-
 * absolute, which counts.)
 */
int nasm_is_simple (nasm_expr *vect) 
{
    while (vect->type && !vect->value)
    	vect++;
    if (!vect->type)
	return 1;
    if (vect->type != EXPR_SIMPLE)
	return 0;
    do {
	vect++;
    } while (vect->type && !vect->value);
    if (vect->type && vect->type < EXPR_SEGBASE+SEG_ABS) return 0;
    return 1;
}

/*
 * Return TRUE if the argument is a simple scalar, _NOT_ a far-
 * absolute.
 */
int nasm_is_really_simple (nasm_expr *vect) 
{
    while (vect->type && !vect->value)
    	vect++;
    if (!vect->type)
	return 1;
    if (vect->type != EXPR_SIMPLE)
	return 0;
    do {
	vect++;
    } while (vect->type && !vect->value);
    if (vect->type) return 0;
    return 1;
}

/*
 * Return TRUE if the argument is relocatable (i.e. a simple
 * scalar, plus at most one segment-base, plus possibly a WRT).
 */
int nasm_is_reloc (nasm_expr *vect) 
{
    while (vect->type && !vect->value) /* skip initial value-0 terms */
    	vect++;
    if (!vect->type)		       /* trivially return TRUE if nothing */
	return 1;		       /* is present apart from value-0s */
    if (vect->type < EXPR_SIMPLE)      /* FALSE if a register is present */
	return 0;
    if (vect->type == EXPR_SIMPLE) {   /* skip over a pure number term... */
	do {
	    vect++;
	} while (vect->type && !vect->value);
	if (!vect->type)	       /* ...returning TRUE if that's all */
	    return 1;
    }
    if (vect->type == EXPR_WRT) {      /* skip over a WRT term... */
	do {
	    vect++;
	} while (vect->type && !vect->value);
	if (!vect->type)	       /* ...returning TRUE if that's all */
	    return 1;
    }
    if (vect->value != 0 && vect->value != 1)
	return 0;		       /* segment base multiplier non-unity */
    do {			       /* skip over _one_ seg-base term... */
	vect++;
    } while (vect->type && !vect->value);
    if (!vect->type)		       /* ...returning TRUE if that's all */
	return 1;
    return 0;			       /* And return FALSE if there's more */
}

/*
 * Return TRUE if the argument contains an `unknown' part.
 */
int nasm_is_unknown(nasm_expr *vect) 
{
    while (vect->type && vect->type < EXPR_UNKNOWN)
	vect++;
    return (vect->type == EXPR_UNKNOWN);
}

/*
 * Return TRUE if the argument contains nothing but an `unknown'
 * part.
 */
int nasm_is_just_unknown(nasm_expr *vect) 
{
    while (vect->type && !vect->value)
	vect++;
    return (vect->type == EXPR_UNKNOWN);
}

/*
 * Return the scalar part of a relocatable vector. (Including
 * simple scalar vectors - those qualify as relocatable.)
 */
long nasm_reloc_value (nasm_expr *vect) 
{
    while (vect->type && !vect->value)
    	vect++;
    if (!vect->type) return 0;
    if (vect->type == EXPR_SIMPLE)
	return vect->value;
    else
	return 0;
}

/*
 * Return the segment number of a relocatable vector, or NO_SEG for
 * simple scalars.
 */
long nasm_reloc_seg (nasm_expr *vect) 
{
    while (vect->type && (vect->type == EXPR_WRT || !vect->value))
    	vect++;
    if (vect->type == EXPR_SIMPLE) {
	do {
	    vect++;
	} while (vect->type && (vect->type == EXPR_WRT || !vect->value));
    }
    if (!vect->type)
	return NO_SEG;
    else
	return vect->type - EXPR_SEGBASE;
}

/*
 * Return the WRT segment number of a relocatable vector, or NO_SEG
 * if no WRT part is present.
 */
long nasm_reloc_wrt (nasm_expr *vect) 
{
    while (vect->type && vect->type < EXPR_WRT)
    	vect++;
    if (vect->type == EXPR_WRT) {
	return vect->value;
    } else
	return NO_SEG;
}

/*
 * Binary search.
 */
int nasm_bsi (char *string, const char **array, int size) 
{
    int i = -1, j = size;	       /* always, i < index < j */
    while (j-i >= 2) {
	int k = (i+j)/2;
	int l = strcmp(string, array[k]);
	if (l<0)		       /* it's in the first half */
	    j = k;
	else if (l>0)		       /* it's in the second half */
	    i = k;
	else			       /* we've got it :) */
	    return k;
    }
    return -1;			       /* we haven't got it :( */
}

static char *file_name = NULL;
static long line_number = 0;

char *nasm_src_set_fname(char *newname) 
{
    char *oldname = file_name;
    file_name = newname;
    return oldname;
}

long nasm_src_set_linnum(long newline) 
{
    long oldline = line_number;
    line_number = newline;
    return oldline;
}

long nasm_src_get_linnum(void) 
{
    return line_number;
}

int nasm_src_get(long *xline, char **xname) 
{
    if (!file_name || !*xname || strcmp(*xname, file_name)) 
    {
	nasm_free(*xname);
	*xname = file_name ? nasm_strdup(file_name) : NULL;
	*xline = line_number;
	return -2;
    }
    if (*xline != line_number) 
    {
	long tmp = line_number - *xline;
	*xline = line_number;
	return tmp;
    }
    return 0;
}

void nasm_quote(char **str) 
{
    size_t ln=strlen(*str);
    char q=(*str)[0];
    char *p;
    if (ln>1 && (*str)[ln-1]==q && (q=='"' || q=='\''))
	return;
    q = '"';
    if (strchr(*str,q))
	q = '\'';
    p = nasm_malloc(ln+3);
    strcpy(p+1, *str);
    nasm_free(*str);
    p[ln+1] = p[0] = q;
    p[ln+2] = 0;
    *str = p;
}
    
char *nasm_strcat(char *one, char *two) 
{
    char *rslt;
    int l1=strlen(one);
    rslt = nasm_malloc(l1+strlen(two)+1);
    strcpy(rslt, one);
    strcpy(rslt+l1, two);
    return rslt;
}
