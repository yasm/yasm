#ifndef	re2c_globals_h
#define	re2c_globals_h

#include "tools/re2c/basics.h"

extern const char *fileName;
extern int sFlag;
extern int bFlag;
extern unsigned int oline;

extern unsigned char asc2ebc[256];
extern unsigned char ebc2asc[256];

extern unsigned char *xlat, *talx;

#endif
