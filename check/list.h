#ifndef LIST_H
#define LIST_H

/*
  Check: a unit test framework for C
  Copyright (C) 2001, Arien Malec

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

typedef struct List List;

/* Create an empty list */
List * list_create (void);

/* Is list at end? */
int list_at_end (List * lp);

/* Position list at front */
void list_front(List *lp);


/* Add a value to the end of the list,
   positioning newly added value as current value */
void list_add_end (List *lp, void *val);

/* Give the value of the current node */
void *list_val (List * lp);

/* Position the list at the next node */
void list_advance (List * lp);

/* Free a list, but don't free values */
void list_free (List * lp);

/* Free a list, freeing values using a freeing function */
/* void list_vfree (List * lp, void (*fp) (void *)); */

#endif /*LIST_H*/
