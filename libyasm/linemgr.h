/* $IdPath$
 * Line manager (for parse stage) header file
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
#ifndef YASM_LINEMGR_H
#define YASM_LINEMGR_H

struct linemgr {
    /* Initialize cur_lindex and any manager internal data structures. */
    void (*initialize) (/*@exits@*/
			void (*error_func) (const char *file,
					    unsigned int line,
					    const char *message));

    /* Cleans up any memory allocated by initialize. */
    void (*cleanup) (void);

    /* Returns the current line index. */
    unsigned long (*get_current) (void);

    /* Goes to the next line (increments the current line index), returns
     * the current (new) line index.
     */
    unsigned long (*goto_next) (void);

    /* Sets a new file/line association starting point at the current line
     * index.  line_inc indicates how much the "real" line is incremented by
     * for each line index increment (0 is perfectly legal).
     */
    void (*set) (const char *filename, unsigned long line,
		 unsigned long line_inc);

    /* Look up the associated actual file and line for a line index. */
    void (*lookup) (unsigned long lindex, /*@out@*/ const char **filename,
		    /*@out@*/ unsigned long *line);
};

#endif
