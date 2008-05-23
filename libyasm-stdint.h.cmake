#ifndef YASM_STDINT_H
#define YASM_STDINT_H

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

#ifndef YASM_LIB_DECL
# ifdef _MSC_VER
#  ifdef YASM_LIB_SOURCE
#   define YASM_LIB_DECL __declspec(dllexport)
#  else
#   define YASM_LIB_DECL __declspec(dllimport)
#  endif
# else
#   define YASM_LIB_DECL
# endif
#endif

#undef HAVE_STDINT_H

#endif
