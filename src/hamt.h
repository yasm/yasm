/* $IdPath$
 * Hash Array Mapped Trie (HAMT) header file.
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of other contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
#ifndef YASM_HAMT_H
#define YASM_HAMT_H

typedef struct HAMT HAMT;

/* Creates new, empty, HAMT.  error_func() is called when an internal error is
 * encountered--it should NOT return to the calling function.
 */
HAMT *HAMT_new(/*@exits@*/ void (*error_func) (const char *file,
					       unsigned int line,
					       const char *message));

/* Deletes HAMT and all data associated with it using deletefunc() on each data
 * item.
 */
void HAMT_delete(/*@only@*/ HAMT *hamt,
		 void (*deletefunc) (/*@only@*/ void *data));

/* Inserts key into HAMT, associating it with data. 
 * If the key is not present in the HAMT, inserts it, sets *replace to 1, and
 *  returns the data passed in.
 * If the key is already present and *replace is 0, deletes the data passed
 *  in using deletefunc() and returns the data currently associated with the
 *  key.
 * If the key is already present and *replace is 1, deletes the data currently
 *  associated with the key using deletefunc() and replaces it with the data
 *  passed in.
 */
/*@dependent@*/ void *HAMT_insert(HAMT *hamt, /*@dependent@*/ const char *str,
				  /*@only@*/ void *data, int *replace,
				  void (*deletefunc) (/*@only@*/ void *data));

/* Searches for the data associated with a key in the HAMT.  If the key is not
 * present, returns NULL.
 */
/*@dependent@*/ /*@null@*/ void *HAMT_search(HAMT *hamt, const char *str);

/* Traverse over all keys in HAMT, calling func() for each data item. 
 * Stops early if func returns 0.
 */
int HAMT_traverse(HAMT *hamt, /*@null@*/ void *d,
		  int (*func) (/*@dependent@*/ /*@null@*/ void *node,
			       /*@null@*/ void *d));

#endif
