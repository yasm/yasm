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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "list.h"
#include "check.h"
#include "check_impl.h"
#include "check_msg.h"


Suite *suite_create (char *name)
{
  Suite *s;
  s = emalloc (sizeof(Suite)); /* freed in suite_free */
  if (name == NULL)
    s->name = "";
  else
    s->name = name;
  s->tclst = list_create();
  return s;
}

void suite_free (Suite *s)
{
  List *l;
  if (s == NULL)
    return;
  for (l = s->tclst; !list_at_end(l); list_advance (l)) {
    tcase_free (list_val(l));
  }
  list_free (s->tclst);
  free(s);
}

TCase *tcase_create (char *name)
{
  TCase *tc = emalloc (sizeof(TCase)); /*freed in tcase_free */
  if (name == NULL)
    tc->name = "";
  else
    tc->name = name;
  tc->tflst = list_create();
  tc->setup = tc->teardown = NULL;

  return tc;
}


void tcase_free (TCase *tc)
{
  List *l;
  l = tc->tflst;
  for (list_front(l); !list_at_end(l); list_advance(l)) {
    free (list_val(l));
  }
  list_free(tc->tflst);
  free(tc);
}

void suite_add_tcase (Suite *s, TCase *tc)
{
  if (s == NULL || tc == NULL)
    return;
  list_add_end (s->tclst, tc);
}

void _tcase_add_test (TCase *tc, TFun fn, char *name)
{
  TF * tf;
  if (tc == NULL || fn == NULL || name == NULL)
    return;
  tf = emalloc (sizeof(TF)); /* freed in tcase_free */
  tf->fn = fn;
  tf->name = name;
  list_add_end (tc->tflst, tf);
}

void tcase_set_fixture (TCase *tc, SFun setup, SFun teardown)
{
  tc->setup = setup;
  tc->teardown = teardown;
}


void tcase_fn_start (int msqid, char *fname, char *file, int line)
{
  send_last_loc_msg (msqid, file, line);
}

void _mark_point (int msqid, char *file, int line)
{
  send_last_loc_msg (msqid, file, line);
}

void _fail_unless (int msqid, int result, char *file, int line, char * msg)
{
  if (line > MAXLINE)
    eprintf ("Line number %d too large to use", line);

  send_last_loc_msg (msqid, file, line);
  if (!result) {
    send_failure_msg (msqid, msg);
    exit(1);
  }
}
