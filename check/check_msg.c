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

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "list.h"
#include "error.h"
#include "check.h"
#include "check_impl.h"
#include "check_msg.h"

enum {
  LASTLOCMSG = 1,
  FAILUREMSG = 2
};
static LastLocMsg *create_last_loc_msg (char *file, int line);
static FailureMsg *create_failure_msg (char *msg);

static FailureMsg *create_failure_msg (char *msg)
{
  FailureMsg *m = emalloc (sizeof(FailureMsg));
  m->message_type = (long int) FAILUREMSG;
  strncpy(m->msg, msg, CMAXMSG);
  return m;
}


static LastLocMsg *create_last_loc_msg (char *file, int line)
{
  LastLocMsg *m = emalloc (sizeof(LastLocMsg));
  m->message_type = (long int) LASTLOCMSG;
  snprintf(m->msg, CMAXMSG, "%s\n%d", file, line);

  return m;
}

char *last_loc_file (LastLocMsg *msg)
{
  int i;
  char *rmsg = emalloc (CMAXMSG); /* caller responsible for freeing */
  char *mmsg = msg->msg;
  if (msg == NULL)
    return NULL;
  for (i = 0; mmsg[i] != '\n'; i++) {
    if (mmsg[i] == '\0')
      eprintf ("Badly formated last loc message");
    rmsg[i] = mmsg[i];
  }
  rmsg[i] = '\0';
  return rmsg;
}

int last_loc_line (LastLocMsg *msg)
{
  char *rmsg;
  if (msg == NULL)
    return -1;
  rmsg  = msg->msg;
  while (*rmsg != '\n') {
    if (*rmsg == '\0')
      eprintf ("Badly formated last loc message");
    rmsg++;
  }
  rmsg++; /*advance past \n */
  return atoi (rmsg);
}


void send_last_loc_msg (int msqid, char * file, int line)
{
  int rval;
  LastLocMsg *rmsg = create_last_loc_msg(file, line);
  rval = msgsnd(msqid, (void *) rmsg, CMAXMSG, IPC_NOWAIT);
  if (rval == -1) {
    eprintf ("send_last_loc_msg:Failed to send message, msqid = %d:",msqid);
  }
  free(rmsg);
}

int create_msq (void) {
  int msqid;
  msqid = msgget((key_t) 1, 0666 | IPC_CREAT);
  if (msqid == -1)
    eprintf ("Unable to create message queue:");
  return msqid;
}

void delete_msq (int msqid)
{
  if (msgctl (msqid, IPC_RMID, NULL) == -1)
    eprintf ("Failed to free message queue:");
}


void send_failure_msg (int msqid, char *msg)
{
  int rval;
  
  FailureMsg *rmsg = create_failure_msg(msg);
  
  rval = msgsnd(msqid, (void *) rmsg, CMAXMSG, IPC_NOWAIT);
  if (rval == -1)
    eprintf ("send_failure_msg:Failed to send message:");
  free(rmsg);
}

LastLocMsg *receive_last_loc_msg (int msqid)
{
  LastLocMsg *rmsg = emalloc(sizeof(LastLocMsg)); /* caller responsible for freeing */
  while (1) {
    int rval;
    rval = msgrcv(msqid, (void *) rmsg, CMAXMSG, LASTLOCMSG, IPC_NOWAIT);
    if (rval == -1) {
      if (errno == ENOMSG)
	break;
      eprintf ("receive_last_loc_msg:Failed to receive message:");
    }
  }
  return rmsg;
}
  
FailureMsg *receive_failure_msg (int msqid)
{ 
  FailureMsg *rmsg = emalloc(sizeof(FailureMsg));
  int rval;
  rval = msgrcv(msqid, (void *) rmsg, CMAXMSG, FAILUREMSG, IPC_NOWAIT);
  if (rval == -1) {
    if (errno == ENOMSG)
      return NULL;
    eprintf ("receive_failure_msg:Failed to receive message:");
  }
  return rmsg;
}

