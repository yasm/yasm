/* $Id: basic-optimizer.c,v 1.1 2001/09/16 21:39:58 peter Exp $
 * Debugging optimizer (used to debug optimizer module interface)
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

#include "optimizer.h"

RCSID("$Id: basic-optimizer.c,v 1.1 2001/09/16 21:39:58 peter Exp $");

/* Define optimizer structure -- see optimizer.h for details */
optimizer dbg_optimizer = {
    "Trace of all info passed to optimizer module",
    "dbg"
};
