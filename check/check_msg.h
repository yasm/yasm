#ifndef CHECK_MSG_H
#define CHECK_MSG_H

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

/* Functions implementing messaging during test runs */
/* check.h must be included before this header */


typedef struct LastLocMsg {
  long int message_type;
  char msg[CMAXMSG]; /* Format: filename\nlineno\0 */
} LastLocMsg;

typedef struct FailureMsg {
  long int message_type;
  char msg[CMAXMSG];
} FailureMsg;

int create_msq (void);
void delete_msq (int msqid);

void send_failure_msg (int msqid, char *msg);
void send_last_loc_msg (int msqid, char * file, int line);

/* malloc'd return value which caller is responsible for
   freeing in each of the next two functions */
FailureMsg *receive_failure_msg (int msqid);
LastLocMsg *receive_last_loc_msg (int msqid);

/* file name contained in the LastLocMsg */
/* return value is malloc'd, caller responsible for freeing */
char *last_loc_file(LastLocMsg *msg);
int last_loc_line(LastLocMsg *msg);

#endif /*CHECK_MSG_H */
