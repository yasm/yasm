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

typedef struct yasm_valparam {
    /*@reldef@*/ STAILQ_ENTRY(yasm_valparam) link;
    /*@owned@*/ /*@null@*/ char *val;
    /*@owned@*/ /*@null@*/ yasm_expr *param;
} yasm_valparam;
typedef /*@reldef@*/ STAILQ_HEAD(yasm_valparamhead, yasm_valparam)
    yasm_valparamhead;

void yasm_vp_new(/*@out@*/ yasm_valparam *r, /*@keep@*/ const char *v,
		 /*@keep@*/ yasm_expr *p);
#define yasm_vp_new(r, v, p)	    do {		\
	r = yasm_xmalloc(sizeof(yasm_valparam));	\
	r->val = v;					\
	r->param = p;					\
    } while(0)

/* void yasm_vps_initialize(//@out@// yasm_valparamhead *headp); */
#define yasm_vps_initialize(headp)	STAILQ_INIT(headp)
void yasm_vps_delete(yasm_valparamhead *headp);
void yasm_vps_append(yasm_valparamhead *headp, /*@keep@*/ yasm_valparam *vp);
#define yasm_vps_append(headp, vp)	do {	    \
	if (vp)					    \
	    STAILQ_INSERT_TAIL(headp, vp, link);    \
    } while(0)

/* //@null@// //@dependent@// yasm_valparam *yasm_vps_first
    (yasm_valparamhead *headp); */
#define yasm_vps_first(headp)	    STAILQ_FIRST(headp)

/* //@null@// //@dependent@// yasm_valparam *yasm_vps_next
    (yasm_valparam *cur); */
#define yasm_vps_next(cur)	    STAILQ_NEXT(cur, link)

#define yasm_vps_foreach(iter, headp)	STAILQ_FOREACH(iter, headp, link)

void yasm_vps_print(FILE *f, /*@null@*/ const yasm_valparamhead *headp);

#endif
