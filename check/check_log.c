#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "list.h"
#include "error.h"
#include "check_impl.h"
#include "check_log.h"
#include "check_print.h"

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

static void srunner_send_evt (SRunner *sr, void *obj, enum cl_event evt);

void srunner_set_log (SRunner *sr, char *fname)
{
  if (sr->log_fname)
    return;
  sr->log_fname = fname;
}

int srunner_has_log (SRunner *sr)
{
  return sr->log_fname != NULL;
}

char *srunner_log_fname (SRunner *sr)
{
  return sr->log_fname;
}

void srunner_register_lfun (SRunner *sr, FILE *lfile, int close,
			    LFun lfun, enum print_verbosity printmode)
{
  Log *l = emalloc (sizeof(Log));
  l->lfile = lfile;
  l->lfun = lfun;
  l->close = close;
  l->mode = printmode;
  list_add_end (sr->loglst, l);
  return;
}

void log_srunner_start (SRunner *sr)
{
  srunner_send_evt (sr, NULL, CLSTART_SR);
}

void log_srunner_end (SRunner *sr)
{
  srunner_send_evt (sr, NULL, CLEND_SR);
}

void log_suite_start (SRunner *sr, Suite *s)
{
  srunner_send_evt (sr, s, CLSTART_S);
}

void log_suite_end (SRunner *sr, Suite *s)
{
  srunner_send_evt (sr, s, CLEND_S);
}

void log_test_end (SRunner *sr, TestResult *tr)
{
  srunner_send_evt (sr, tr, CLEND_T);
}

static void srunner_send_evt (SRunner *sr, void *obj, enum cl_event evt)
{
  List *l;
  Log *lg;
  l = sr->loglst;
  for (list_front(l); !list_at_end(l); list_advance(l)) {
    lg = list_val(l);
    fflush(lg->lfile);
    lg->lfun (sr, lg->lfile, lg->mode, obj, evt);
    fflush(lg->lfile);
  }
}

void stdout_lfun (SRunner *sr, FILE *file, enum print_verbosity printmode,
		  void *obj, enum cl_event evt)
{
  TestResult *tr;
  Suite *s;
  
  switch (evt) {
  case CLSTART_SR:
    if (printmode > CRSILENT) {
      fprintf(file, "Running suite(s):");
    }
    break;
  case CLSTART_S:
    s = obj;
    if (printmode > CRSILENT) {
      fprintf(file, " %s", s->name);
    }
    break;
  case CLEND_SR:
    if (printmode > CRSILENT) {
      fprintf (file, "\n");
      srunner_fprint (file, sr, printmode);
    }
    break;
  case CLEND_S:
    s = obj;
    break;
  case CLEND_T:
    tr = obj;
    break;
  default:
    eprintf("Bad event type received in stdout_lfun");
  }

  
}

void lfile_lfun (SRunner *sr, FILE *file, enum print_verbosity printmode,
		 void *obj, enum cl_event evt)
{
  TestResult *tr;
  Suite *s;
  
  switch (evt) {
  case CLSTART_SR:
    break;
  case CLSTART_S:
    s = obj;
    fprintf(file, "Running suite %s\n", s->name);
    break;
  case CLEND_SR:
    fprintf (file, "Results for all suites run:\n");
    srunner_fprint (file, sr, CRMINIMAL);
    break;
  case CLEND_S:
    s = obj;
    break;
  case CLEND_T:
    tr = obj;
    tr_fprint(file, tr, CRVERBOSE);
    break;
  default:
    eprintf("Bad event type received in stdout_lfun");
  }

  
}

FILE *srunner_open_lfile (SRunner *sr)
{
  FILE *f = NULL;
  if (srunner_has_log (sr)) {
    f = fopen(sr->log_fname, "w");
    if (f == NULL)
      eprintf ("Could not open log file %s:", sr->log_fname);
  }
  return f;
}

void srunner_init_logging (SRunner *sr, enum print_verbosity print_mode)
{
  FILE *f;
  sr->loglst = list_create();
  srunner_register_lfun (sr, stdout, 0, stdout_lfun, print_mode);
  f = srunner_open_lfile (sr);
  if (f) {
    srunner_register_lfun (sr, f, 1, lfile_lfun, print_mode);
  }
}

void srunner_end_logging (SRunner *sr)
{
  List *l;
  int rval;

  l = sr->loglst;
  for (list_front(l); !list_at_end(l); list_advance(l)) {
    Log *lg = list_val(l);
    if (lg->close) {
      rval = fclose (lg->lfile);
      if (rval != 0)
	eprintf ("Error closing log file:");
    }
    free (lg);
  }
  list_free(l);
  sr->loglst = NULL;
}
