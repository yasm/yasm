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

#include <stdio.h>
#include <check.h>
#include "list.h"
#include "check_impl.h"
#include "check_print.h"
#include "error.h"

static void srunner_fprint_summary (FILE *file, SRunner *sr, int print_mode);
static void srunner_fprint_results (FILE *file, SRunner *sr, int print_mode);

static int percent_passed (TestStats *t);
static char *rtype_to_string (int rtype);

void srunner_print (SRunner *sr, int print_mode)
{
  srunner_fprint (stdout, sr, print_mode);
}

void srunner_fprint (FILE *file, SRunner *sr, int print_mode)
{
  srunner_fprint_summary (file, sr, print_mode);
  srunner_fprint_results (file, sr, print_mode);
}

static void srunner_fprint_summary (FILE *file, SRunner *sr, int print_mode)
{
  TestStats *ts = sr->stats;
  if (print_mode >= CRMINIMAL) {
    fprintf (file, "%d%%: Checks: %d, Failures: %d, Errors: %d\n",
	     percent_passed (ts), ts->n_checked, ts->n_failed,
	     ts->n_errors);
  }
  return;
}

static void srunner_fprint_results (FILE *file, SRunner *sr, int print_mode)
{
  List *resultlst;
  
  resultlst = sr->resultlst;
  
  for (list_front(resultlst); !list_at_end(resultlst); list_advance(resultlst)) {
    TestResult *tr = list_val(resultlst);
    tr_fprint (file, tr, print_mode);
  }
  return;
}

void tr_fprint (FILE *file, TestResult *tr, int print_mode)
{
  char *exact_msg;
  exact_msg = (tr->rtype == CRERROR) ? "(after this point) ": "";
  if ((print_mode >= CRVERBOSE && tr->rtype == CRPASS) ||
      (tr->rtype != CRPASS && print_mode >= CRNORMAL)) {
    fprintf (file, "%s:%d:%s:%s: %s%s\n",
	     tr->file, tr->line,
	     rtype_to_string(tr->rtype),  tr->tcname,
	     exact_msg, tr->msg);
  }
}

static int percent_passed (TestStats *t)
{
  if (t->n_failed == 0 && t->n_errors == 0)
    return 100;
  else
    return (int) ( (float) (t->n_checked - (t->n_failed + t->n_errors)) /
		   (float) t->n_checked * 100);
}

static char *rtype_to_string (int rtype)
{
  switch (rtype) {
  case CRPASS:
    return "P";
    break;
  case CRFAILURE:
    return "F";
    break;
  case CRERROR:
    return "E";
    break;
  default:
    eprintf("Bad argument %d to rtype_to_string", rtype);
    return NULL;
  }
}
