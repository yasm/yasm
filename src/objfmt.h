/* $IdPath$
 * Object format module interface header file
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
#ifndef YASM_OBJFMT_H
#define YASM_OBJFMT_H

/* Interface to the object format module(s) */
struct objfmt {
    /* one-line description of the format */
    const char *name;

    /* keyword used to select format on the command line */
    const char *keyword;

    /* default (starting) section name */
    const char *default_section_name;

    /* default (starting) BITS setting */
    const unsigned char default_mode_bits;

    /* NULL-terminated list of debugging formats that are valid to use with
     * this object format.
     */
/*    debugfmt **debugfmts;*/

    /* Default debugging format (set even if there's only one available to
     * use)
     */
/*    debugfmt *default_df;*/

    /* Is the specified section name valid?
     * Return is a boolean value.
     */
    int (*is_valid_section) (const char *name);
};

/* Available object formats */
extern objfmt dbg_objfmt;

#endif
