/* $IdPath$
 * Value/Parameter type definition
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
