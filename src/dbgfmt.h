/* $IdPath$
 * Debug format module interface header file
 *
 *  Copyright (C) 2002  Peter Johnson
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
#ifndef YASM_DBGFMT_H
#define YASM_DBGFMT_H

/* Interface to the object format module(s) */
struct dbgfmt {
    /* one-line description of the format */
    const char *name;

    /* keyword used to select format on the command line */
    const char *keyword;

    /* Initializes debug output.  Must be called before any other debug
     * format functions.  The filenames are provided solely for informational
     * purposes.  May be NULL if not needed by the debug format.
     */
    void (*initialize) (const char *in_filename, const char *obj_filename);

    /* Cleans up anything allocated by initialize.
     * May be NULL if not needed by the debug format.
     */
    void (*cleanup) (void);
};

#endif
