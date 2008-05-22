#cmakedefine HAVE_STDINT_H

#ifdef HAVE_STDINT_H
#include <stdint.h>
#elif defined(_MSC_VER)

#ifndef _UINTPTR_T_DEFINED
#ifdef _WIN64
#include <vadefs.h>
#else
typedef unsigned long uintptr_t;
#endif
#define _UINTPTR_T_DEFINED
#endif

#else
typedef unsigned long uintptr_t;
#endif

#undef HAVE_STDINT_H
