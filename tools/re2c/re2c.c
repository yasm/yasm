#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include "tools/re2c/globals.h"
#include "tools/re2c/parse.h"
#include "tools/re2c/dfa.h"

const char *fileName;
int sFlag = 0;
int bFlag = 0;

int main(int argc, char *argv[]){
    FILE *f;

    fileName = NULL;
    if(argc == 1)
	goto usage;
    while(--argc > 1){
	char *p = *++argv;
	while(*++p != '\0'){
	    switch(*p){
	    case 'e':
		xlat = asc2ebc;
		talx = ebc2asc;
		break;
	    case 's':
		sFlag = 1;
		break;
	    case 'b':
		sFlag = 1;
		bFlag = 1;
		break;
	    default:
		goto usage;
	    }
	}
    }
    fileName = *++argv;
    if(fileName[0] == '-' && fileName[1] == '\0'){
	fileName = "<stdin>";
	f = stdin;
    } else {
	if((f = fopen(fileName, "rt")) == NULL){
	    fprintf(stderr, "can't open %s\n", fileName);
	    return 1;
	}
    }
    parse(f, stdout);
    return 0;
usage:
    fputs("usage: re2c [-esb] name\n", stderr);
    return 2;
}
