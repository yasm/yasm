/* $Id: outfmt.h,v 1.1 2001/08/19 02:15:18 peter Exp $
 * Output format module interface header file
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
#ifndef YASM_OUTFMT_H
#define YASM_OUTFMT_H

/* Interface to the output format module(s) */
typedef struct outfmt_s {
    char *name;		/* one-line description of the format */
    char *keyword;	/* keyword used to select format on the command line */

    /* NULL-terminated list of debugging formats that are valid to use with
     * this output format.
     */
/*    struct debugfmt_s **debugfmts;*/

    /* Default debugging format (set even if there's only one available to
     * use)
     */
/*    struct debugfmt_s *default_df;*/
} outfmt;

/* Available output formats */
extern outfmt dbg_outfmt;

#endif
