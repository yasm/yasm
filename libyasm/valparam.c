/*
 * Value/Parameter type functions
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

#include "globals.h"
#include "expr.h"


void
vps_delete(valparamhead *headp)
{
    valparam *cur, *next;

    cur = STAILQ_FIRST(headp);
    while (cur) {
	next = STAILQ_NEXT(cur, link);
	if (cur->val)
	    xfree(cur->val);
	if (cur->param)
	    expr_delete(cur->param);
	xfree(cur);
	cur = next;
    }
    STAILQ_INIT(headp);
}

void
vps_print(FILE *f, const valparamhead *headp)
{
    const valparam *vp;

    if(!headp) {
	fprintf(f, "(none)");
	return;
    }

    vps_foreach(vp, headp) {
	if (vp->val)
	    fprintf(f, "(\"%s\",", vp->val);
	else
	    fprintf(f, "((nil),");
	if (vp->param)
	    expr_print(f, vp->param);
	else
	    fprintf(f, "(nil)");
	fprintf(f, ")");
	if (vps_next(vp))
	    fprintf(f, ",");
    }
}
