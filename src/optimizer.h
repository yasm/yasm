/* $Id: optimizer.h,v 1.1 2001/09/16 21:39:58 peter Exp $
 * Optimizer module interface header file
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
#ifndef YASM_OPTIMIZER_H
#define YASM_OPTIMIZER_H

/* Interface to the optimizer module(s) */
typedef struct optimizer_s {
    /* one-line description of the optimizer */
    char *name;

    /* keyword used to select optimizer on the command line */
    char *keyword;

    /* Main entrance point for the optimizer.
     *
     * This function takes the unoptimized linked list of sections and returns
     * an optimized linked list of sections ready for output to an object file.
     */
} optimizer;

/* Available optimizers */
extern optimizer dbg_optimizer;

#endif
