/*
 * Hash Array Mapped Trie (HAMT) implementation
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 *  Based on the paper "Ideal Hash Tries" by Phil Bagwell [2000].
 *  One algorithmic change from that described in the paper: we use the LSB's
 *  of the key to index the root table and move upward in the key rather than
 *  use the MSBs as described in the paper.  The LSBs have more entropy.
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
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "errwarn.h"
#include "hamt.h"

typedef struct HAMTEntry {
    SLIST_ENTRY(HAMTEntry) next;	/* next hash table entry */
    /*@dependent@*/ const char *str;	/* string being hashed */
    /*@owned@*/ void *data;		/* data pointer being stored */
} HAMTEntry;

typedef struct HAMTNode {
    unsigned long BitMapKey;		/* 32 bits, bitmap or hash key */
    void *BaseValue;			/* Base of HAMTNode list or value */
} HAMTNode;

struct HAMT {
    SLIST_HEAD(HAMTEntryHead, HAMTEntry) entries;
    HAMTNode *root;
};

/* XXX make a portable version of this.  This depends on the pointer being
 * 4 or 2-byte aligned (as it uses the LSB of the pointer variable to store
 * the subtrie flag!
 */
#define IsSubTrie(n)		((unsigned long)((n)->BaseValue) & 1)
#define SetSubTrie(n, v)	do {				\
	if ((unsigned long)(v) & 1)				\
	    InternalError(_("Subtrie is seen as subtrie before flag is set (misaligned?)"));	\
	(n)->BaseValue = (void *)((unsigned long)(v) | 1);	\
    } while (0)
#define GetSubTrie(n)		(HAMTNode *)((unsigned long)((n)->BaseValue)&~1UL)

static unsigned long
HashKey(const char *key)
{
    unsigned long a=31415, b=27183, vHash;
    for (vHash=0; *key; key++, a*=b)
	vHash = a*vHash + *key;
    return vHash;
}

static unsigned long
ReHashKey(const char *key, int Level)
{
    unsigned long a=31415, b=27183, vHash;
    for (vHash=0; *key; key++, a*=b)
	vHash = a*vHash*(unsigned long)Level + *key;
    return vHash;
}

/*@-compdef -nullret@*/
HAMT *
HAMT_new(void)
{
    HAMT *hamt;
    int i;

    hamt = xmalloc(sizeof(HAMT));
    SLIST_INIT(&hamt->entries);
    hamt->root = xmalloc(32*sizeof(HAMTNode));

    for (i=0; i<32; i++)
	hamt->root[i].BaseValue = NULL;

    return hamt;
}
/*@=compdef =nullret@*/

static void
HAMT_delete_trie(HAMTNode *node)
{
    if (IsSubTrie(node)) {
	unsigned long i, Size;

	/* Count total number of bits in bitmap to determine size */
	BitCount(Size, node->BitMapKey);
	Size &= 0x1F;	/* Clamp to <32 */

	for (i=0; i<Size; i++)
	    HAMT_delete_trie(&(GetSubTrie(node))[i]);
	xfree(GetSubTrie(node));
    }
}

void
HAMT_delete(HAMT *hamt, void (*deletefunc) (/*@keep@*/ void *data))
{
    int i;

    /* delete entries */
    while (!SLIST_EMPTY(&hamt->entries)) {
	HAMTEntry *entry;
	entry = SLIST_FIRST(&hamt->entries);
	SLIST_REMOVE_HEAD(&hamt->entries, next);
	deletefunc(entry->data);
	xfree(entry);
    }

    /* delete trie */
    for (i=0; i<32; i++)
	HAMT_delete_trie(&hamt->root[i]);

    xfree(hamt->root);
    xfree(hamt);
}

int
HAMT_traverse(HAMT *hamt, void *d,
	      int (*func) (/*@dependent@*/ /*@null@*/ void *node,
			    /*@null@*/ void *d))
{
    HAMTEntry *entry;
    SLIST_FOREACH(entry, &hamt->entries, next)
	if (func(entry->data, d) == 0)
	    return 0;
    return 1;
}

/*@-temptrans -kepttrans -mustfree@*/
void *
HAMT_insert(HAMT *hamt, const char *str, void *data, int *replace,
	    void (*deletefunc) (/*@only@*/ void *data))
{
    HAMTNode *node, *newnodes;
    HAMTEntry *entry;
    unsigned long key, keypart, Map;
    int keypartbits = 0;
    int level = 0;

    key = HashKey(str);
    keypart = key & 0x1F;
    node = &hamt->root[keypart];

    if (!node->BaseValue) {
	node->BitMapKey = key;
	entry = xmalloc(sizeof(HAMTEntry));
	entry->str = str;
	entry->data = data;
	SLIST_INSERT_HEAD(&hamt->entries, entry, next);
	node->BaseValue = entry;
	if (IsSubTrie(node))
	    InternalError(_("Data is seen as subtrie (misaligned?)"));
	*replace = 1;
	return data;
    }

    for (;;) {
	if (!(IsSubTrie(node))) {
	    if (node->BitMapKey == key) {
		/*@-branchstate@*/
		if (*replace) {
		    deletefunc(((HAMTEntry *)(node->BaseValue))->data);
		    ((HAMTEntry *)(node->BaseValue))->str = str;
		    ((HAMTEntry *)(node->BaseValue))->data = data;
		} else
		    deletefunc(data);
		/*@=branchstate@*/
		return ((HAMTEntry *)(node->BaseValue))->data;
	    } else {
		unsigned long key2 = node->BitMapKey;
		/* build tree downward until keys differ */
		for (;;) {
		    unsigned long keypart2;

		    /* replace node with subtrie */
		    keypartbits += 5;
		    if (keypartbits > 30) {
			/* Exceeded 32 bits: rehash */
			key = ReHashKey(str, level);
			key2 = ReHashKey(((HAMTEntry *)(node->BaseValue))->str,
					 level);
			keypartbits = 0;
		    }
		    keypart = (key >> keypartbits) & 0x1F;
		    keypart2 = (key2 >> keypartbits) & 0x1F;

		    if (keypart == keypart2) {
			/* Still equal, build one-node subtrie and continue
			 * downward.
			 */
			newnodes = xmalloc(sizeof(HAMTNode));
			newnodes[0] = *node;	/* structure copy */
			node->BitMapKey = 1<<keypart;
			SetSubTrie(node, newnodes);
			node = &newnodes[0];
			level++;
		    } else {
			/* partitioned: allocate two-node subtrie */
			newnodes = xmalloc(2*sizeof(HAMTNode));

			entry = xmalloc(sizeof(HAMTEntry));
			entry->str = str;
			entry->data = data;
			SLIST_INSERT_HEAD(&hamt->entries, entry, next);

			/* Copy nodes into subtrie based on order */
			if (keypart2 < keypart) {
			    newnodes[0] = *node;    /* structure copy */
			    newnodes[1].BitMapKey = key;
			    newnodes[1].BaseValue = entry;
			} else {
			    newnodes[0].BitMapKey = key;
			    newnodes[0].BaseValue = entry;
			    newnodes[1] = *node;    /* structure copy */
			}

			/* Set bits in bitmap corresponding to keys */
			node->BitMapKey = (1UL<<keypart) | (1UL<<keypart2);
			SetSubTrie(node, newnodes);
			*replace = 1;
			return data;
		    }
		}
	    }
	}

	/* Subtrie: look up in bitmap */
	keypartbits += 5;
	if (keypartbits > 30) {
	    /* Exceeded 32 bits of current key: rehash */
	    key = ReHashKey(str, level);
	    keypartbits = 0;
	}
	keypart = (key >> keypartbits) & 0x1F;
	if (!(node->BitMapKey & (1<<keypart))) {
	    /* bit is 0 in bitmap -> add node to table */
	    unsigned long Size;

	    /* set bit to 1 */
	    node->BitMapKey |= 1<<keypart;

	    /* Count total number of bits in bitmap to determine new size */
	    BitCount(Size, node->BitMapKey);
	    Size &= 0x1F;	/* Clamp to <32 */
	    newnodes = xmalloc(Size*sizeof(HAMTNode));

	    /* Count bits below to find where to insert new node at */
	    BitCount(Map, node->BitMapKey & ~((~0UL)<<keypart));
	    Map &= 0x1F;	/* Clamp to <32 */
	    /* Copy existing nodes leaving gap for new node */
	    memcpy(newnodes, GetSubTrie(node), Map*sizeof(HAMTNode));
	    memcpy(&newnodes[Map+1], &(GetSubTrie(node))[Map],
		   (Size-Map-1)*sizeof(HAMTNode));
	    /* Delete old subtrie */
	    xfree(GetSubTrie(node));
	    /* Set up new node */
	    newnodes[Map].BitMapKey = key;
	    entry = xmalloc(sizeof(HAMTEntry));
	    entry->str = str;
	    entry->data = data;
	    SLIST_INSERT_HEAD(&hamt->entries, entry, next);
	    newnodes[Map].BaseValue = entry;
	    SetSubTrie(node, newnodes);

	    *replace = 1;
	    return data;
	}

	/* Count bits below */
	BitCount(Map, node->BitMapKey & ~((~0UL)<<keypart));
	Map &= 0x1F;	/* Clamp to <32 */

	/* Go down a level */
	level++;
	node = &(GetSubTrie(node))[Map];
    }
}
/*@=temptrans =kepttrans =mustfree@*/

void *
HAMT_search(HAMT *hamt, const char *str)
{
    HAMTNode *node;
    unsigned long key, keypart, Map;
    int keypartbits = 0;
    int level = 0;
    
    key = HashKey(str);
    keypart = key & 0x1F;
    node = &hamt->root[keypart];

    if (!node->BaseValue)
	return NULL;

    for (;;) {
	if (!(IsSubTrie(node))) {
	    if (node->BitMapKey == key)
		return ((HAMTEntry *)(node->BaseValue))->data;
	    else
		return NULL;
	}

	/* Subtree: look up in bitmap */
	keypartbits += 5;
	if (keypartbits > 30) {
	    /* Exceeded 32 bits of current key: rehash */
	    key = ReHashKey(str, level);
	    keypartbits = 0;
	}
	keypart = (key >> keypartbits) & 0x1F;
	if (!(node->BitMapKey & (1<<keypart)))
	    return NULL;	/* bit is 0 in bitmap -> no match */

	/* Count bits below */
	BitCount(Map, node->BitMapKey & ~((~0UL)<<keypart));
	Map &= 0x1F;	/* Clamp to <32 */

	/* Go down a level */
	level++;
	node = &(GetSubTrie(node))[Map];
    }
}

