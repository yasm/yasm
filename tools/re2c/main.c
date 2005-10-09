#include <stdio.h>
#include <stdlib.h>

#include "globals.h"
#include "parse.h"
#include "dfa.h"
#include "mbo_getopt.h"

const char *fileName = 0;
const char *outputFileName = 0;
int sFlag = 0;
int bFlag = 0;
int bUsedYYAccept = 0;
unsigned int oline = 1;
unsigned int maxFill = 1;
int vFillIndexes = -1;
unsigned char *vUsedLabels;
unsigned int vUsedLabelAlloc = 1000;

static char *opt_arg = NULL;
static int opt_ind = 1;

static const mbo_opt_struct OPTIONS[] = {
	{'?', 0, "help"},
	{'b', 0, "bit-vectors"},
	{'e', 0, "ecb"},
	{'h', 0, "help"},
	{'s', 0, "nested-ifs"},
	{'f', 0, "storable-state"},
	{'o', 1, "output"},
	{'v', 0, "version"},
        {'-', 0, NULL} /* end of args */ 
};

static void usage()
{
    fprintf(stderr,
	"usage: re2c [-esbvh] file\n"
	"\n"
	"-? -h   --help          Display this info.\n"
	"\n"
	"-b      --bit-vectors   Implies -s. Use bit vectors as well in the attempt to\n"
	"                        coax better code out of the compiler. Most useful for\n"
	"                        specifications with more than a few keywords (e.g. for\n"
	"                        most programming languages).\n"
	"\n"
	"-e      --ecb           Cross-compile from an ASCII platform to\n"
	"                        an EBCDIC one.\n"
	"\n"
	"-s      --nested-ifs    Generate nested ifs for some switches. Many compilers\n"
	"                        need this assist to generate better code.\n"
	"\n"
	"-f      --storable-state Generate a scanner with support for storable state\n"
	"\n"
	"-o      --output=output Specify the output file instead of stdout\n"
	"\n"
	"-v      --version       Show version information.\n"
	"-V      --vernum        Show version as one number.\n");
}

int main(int argc, char *argv[])
{
    int c;
    FILE *f, *output;

    fileName = NULL;

    if(argc == 1) {
	usage();
	return 2;
    }

    while ((c = mbo_getopt(argc, argv, OPTIONS, &opt_arg, &opt_ind, 0))!=-1) {
	switch (c) {
	    case 'b':
		sFlag = 1;
		bFlag = 1;
		break;
	    case 'e':
		xlat = asc2ebc;
		talx = ebc2asc;
		break;
	    case 's':
		sFlag = 1;
		break;
	    case 'f':
		vFillIndexes = 0;
		break;
	    case 'o':
		outputFileName = opt_arg;
		break;
	    case 'v':
		fputs("re2c " PACKAGE_VERSION "\n", stdout);
		break;
	    case 'V': {
		int v1, v2, v3;
		sscanf(PACKAGE_VERSION, "%d.%d.%d", &v1, &v2, &v3);
		fprintf(stdout, "%02d%02d%02d\n", v1, v2, v3);
		return 2;
	    }
	    case 'h':
	    case '?':
	    default:
		usage();
		return 2;
 	}
    }

    if (argc == opt_ind + 1) {
	fileName = argv[opt_ind];
    } else {
	usage();
	return 2;
    }

    vUsedLabels = calloc(vUsedLabelAlloc, 1);
    if (!vUsedLabels) {
	fputs("Out of memory.\n", stderr);
	return 1;
    }

    /* set up the input stream */
    if(fileName[0] == '-' && fileName[1] == '\0'){
	fileName = "<stdin>";
	f = stdin;
    } else {
	if((f = fopen(fileName, "rt")) == NULL){
	    fprintf(stderr, "can't open %s\n", fileName);
	    return 1;
	}
    }

    // set up the output stream
    if (outputFileName == 0 || (fileName[0] == '-' && fileName[1] == '\0')) {
	outputFileName = "<stdout>";
	output = stdout;
    } else {
	output = fopen(outputFileName, "wt");
	if (!output) {
	    fprintf(stderr, "can't open %s\n", outputFileName);
	    return 1;
	}
    }

    parse(f, output);
    return 0;
}
