/*
 ---------------------------------------------------------------------------
 Copyright (c) 1998-2007, Brian Gladman, Worcester, UK. All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software is allowed (with or without
 changes) provided that:

  1. source code distributions include the above copyright notice, this
     list of conditions and the following disclaimer;

  2. binary distributions include the above copyright notice, this list
     of conditions and the following disclaimer in their documentation;

  3. the name of the copyright holder is not used to endorse products
     built using this software without specific written permission.

 DISCLAIMER

 This software is provided 'as is' with no explicit or implied warranties
 in respect of its properties, including, but not limited to, correctness
 and/or fitness for purpose.
 ---------------------------------------------------------------------------

 This program converts Visual Studio 2008 C/C++ version 9 build projects 
 into build projects for Visual Studio 2005 C/C++ version 8.    It will 
 also convert files that have been converted in this way back into their
 original form.  It does this conversion by looking for *.vcproj files 
 in the current working directory and its sub-directories and changing 
 the following line in each of them:

    Version="9.00"

 to:

    Version="8.00"

 or vice versa.
 
 It is used by compiling it, placing it in a root build directory and 
 running it from there.  It acts recursively on all sub-directories of
 this directory it is important not to run it at a directory level in
 which not all projects are to be converted.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>

void process_file(char *fn)
{   FILE *f;
    fpos_t pos;
    char iobuf[1024], *p;

	if((f = fopen(fn, "r+")) == 0)  /* open for read and write  */
	{
		printf("File open error on: %s\n", fn); return;
	}

    for( ; ; )
    {
        if(feof(f))                 /* version line not found   */
            break;

        if(fgetpos(f, &pos))        /* save position of line    */
        {
    		printf("File getpos error on: %s\n", fn); break;
        }

        if(!fgets(iobuf, 1024, f))  /* read the line in         */
        {
    		printf("File read error on: %s\n", fn); break;
        }

        if(p = strchr(iobuf, 'V'))  /* look for 'version' line  */
        {
            if(!strncmp(p, "Version=\"8.00\"", 14) 
                        || !strncmp(p, "Version=\"9.00\"", 14))
            {
                *(p + 9) ^= 1;      /* switch versions          */
                /* reset back to start of line in the file and  */
                /* write the changed line to the file           */
                if(fsetpos(f, &pos))
		            printf("File setpos error on: %s\n", fn);
                else if(fputs(iobuf, f) == EOF)
		            printf("File write error on: %s\n", fn);
                break;              /* this file is now fixed   */   
            }
        }
    }
    fclose(f);
}

void find_vcproj_files(char *dir)
{   struct _finddata_t cfile;
    intptr_t hfile;
    char nbuf[_MAX_PATH], *p;

	strcpy(nbuf, dir);          /* find directories and files   */
	strcat(nbuf, "\\*.*");
    if((hfile = _findfirst( nbuf, &cfile )) == -1L )
		return;                 /* if directory is empty        */

	do
	{
		strcpy(nbuf, dir);      /* compile path to the current  */
		strcat(nbuf, "\\");     /* file or directory            */
		strcat(nbuf, cfile.name);

		if((cfile.attrib & _A_SUBDIR) == _A_SUBDIR)
		{                       /* if it is a sub-direcory      */
            if(strcmp(cfile.name, ".") && strcmp(cfile.name, ".."))
				find_vcproj_files(nbuf);    /* recursive search */
		}
		else if((cfile.attrib & 0x1f) == _A_NORMAL && (p = strrchr(cfile.name, '.'))
					&& !strcmp(p, ".vcproj"))
            process_file(nbuf); /* if it is a *.vcproj file     */
	}
    while
        ( _findnext( hfile, &cfile ) == 0 );

    _findclose( hfile );
}

int main(void)
{	char *dir;

    if(!(dir = _getcwd( NULL, 0 )))
		printf("Cannot obtain current working directory\n");
    else
        find_vcproj_files(dir);
}
