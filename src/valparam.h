/* $IdPath$
 * Value/Parameter type definition
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of other contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
#ifndef YASM_VALPARAM_H
#define YASM_VALPARAM_H

typedef struct valparam {
    /*@reldef@*/ STAILQ_ENTRY(valparam) link;
    /*@owned@*/ /*@null@*/ char *val;
    /*@owned@*/ /*@null@*/ expr *param;
} valparam;
typedef /*@reldef@*/ STAILQ_HEAD(valparamhead, valparam) valparamhead;

void vp_new(/*@out@*/ valparam *r, /*@keep@*/ const char *v,
	    /*@keep@*/ expr *p);
#define vp_new(r, v, p)		do {	\
	r = xmalloc(sizeof(valparam));	\
	r->val = v;			\
	r->param = p;			\
    } while(0)

void vps_initialize(/*@out@*/ valparamhead *headp);
#define vps_initialize(headp)	STAILQ_INIT(headp)
void vps_delete(valparamhead *headp);
void vps_append(valparamhead *headp, /*@keep@*/ valparam *vp);
#define vps_append(headp, vp)	do {		    \
	if (vp)					    \
	    STAILQ_INSERT_TAIL(headp, vp, link);    \
    } while(0)

/*@null@*/ /*@dependent@*/ valparam *vps_first(valparamhead *headp);
#define vps_first(headp)	    STAILQ_FIRST(headp)

/*@null@*/ /*@dependent@*/ valparam *vps_next(valparam *cur);
#define vps_next(cur)		    STAILQ_NEXT(cur, link)

#define vps_foreach(iter, headp)    STAILQ_FOREACH(iter, headp, link)

void vps_print(FILE *f, /*@null@*/ const valparamhead *headp);

#endif
