/* $IdPath$
 * Hash Array Mapped Trie (HAMT) header file.
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
#ifndef YASM_HAMT_H
#define YASM_HAMT_H

typedef struct HAMT HAMT;

/* Creates new, empty, HAMT. */
HAMT *HAMT_new(void);

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
