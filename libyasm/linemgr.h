/* $IdPath$
 * YASM virtual line mapping management functions
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

/* Create a new line mapping repository. */
yasm_linemap *yasm_linemap_create(void);

/* Cleans up any memory allocated. */
void yasm_linemap_destroy(yasm_linemap *linemap);

/* Returns the current line index. */
unsigned long yasm_linemap_get_current(yasm_linemap *linemap);

/** Get associated data for a virtual line and data callback.
 * \param linemap   linemap
 * \param line	    virtual line
 * \param callback  callback used when adding data
 * \return Associated data (NULL if none).
 */
/*@dependent@*/ /*@null@*/ void *yasm_linemap_get_data
    (yasm_linemap *linemap, unsigned long line,
     const yasm_assoc_data_callback *callback);

/** Add associated data to the current virtual line.
 * \attention Deletes any existing associated data for that data callback for
 *	      the current virtual line.
 * \param linemap	linemap
 * \param callback	callback
 * \param data		data to associate
 * \param every_hint	non-zero if data is likely to be associated with every
 *			line; zero if not.
 */
void yasm_linemap_add_data(yasm_linemap *linemap,
			   const yasm_assoc_data_callback *callback,
			   /*@only@*/ /*@null@*/ void *data, int every_hint);

/* Goes to the next line (increments the current virtual line), returns
 * the current (new) virtual line.
 */
unsigned long yasm_linemap_goto_next(yasm_linemap *linemap);

/* Sets a new file/line physical association starting point at the current
 * virtual line.  line_inc indicates how much the "real" line is incremented
 * by for each virtual line increment (0 is perfectly legal).
 */
void yasm_linemap_set(yasm_linemap *linemap, const char *filename,
		      unsigned long file_line, unsigned long line_inc);

/* Look up the associated physical file and line for a virtual line. */
void yasm_linemap_lookup(yasm_linemap *linemap, unsigned long line,
			 /*@out@*/ const char **filename,
			 /*@out@*/ unsigned long *file_line);
#endif
