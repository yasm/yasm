/* $IdPath$
 * YASM line manager (for parse stage) header file
 *
 *  Copyright (C) 2002  Peter Johnson
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
#ifndef YASM_LINEMGR_H
#define YASM_LINEMGR_H

/* Standard data types appropriate for use with add_assoc_data(). */
typedef enum yasm_linemgr_std_type {
    /* Source line, a 0-terminated, allocated string. */
    YASM_LINEMGR_STD_TYPE_SOURCE = 1,   
    /* User-defined types start here.  Use odd numbers (low bit set) for types
     * very likely to have data associated for every line.
     */
    YASM_LINEMGR_STD_TYPE_USER = 4
} yasm_linemgr_std_type;

struct yasm_linemgr {
    /* Initialize cur_lindex and any manager internal data structures. */
    void (*initialize) (/*@exits@*/
			void (*error_func) (const char *file,
					    unsigned int line,
					    const char *message));

    /* Cleans up any memory allocated. */
    void (*cleanup) (void);

    /* Returns the current line index. */
    unsigned long (*get_current) (void);

    /* Associates data with the current line index.
     * Deletes old data associated with type if present.
     * The function delete_func is used to delete the data (if non-NULL).
     * All data of a particular type needs to have the exact same deletion
     * function specified to this function on every call.
     */
    void (*add_assoc_data) (int type, /*@only@*/ void *data,
			    /*@null@*/ void (*delete_func) (void *));

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

    /* Returns data associated with line index and type.
     * Returns NULL if no data of that type was associated with that line.
     */
    /*@dependent@*/ /*@null@*/ void * (*lookup_data) (unsigned long lindex,
						      int type);
};

#endif
