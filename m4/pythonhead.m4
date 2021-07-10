dnl a macro to check for ability to create python extensions
dnl  AM_CHECK_PYTHON_HEADERS([ACTION-IF-POSSIBLE], [ACTION-IF-NOT-POSSIBLE])
dnl function also defines PYTHON_INCLUDES
dnl update taken from dbus-python(GPL).
AC_DEFUN([AM_CHECK_PYTHON_HEADERS],
[AC_REQUIRE([AM_PATH_PYTHON])

AC_PATH_PROGS([PYTHON_CONFIG], [python${PYTHON_VERSION}-config python-config], [no])
if test "${PYTHON_CONFIG}" = "no"; then
  AC_MSG_ERROR([cannot find python${PYTHON_VERSION}-config or python-config in PATH])
fi

AC_ARG_VAR([PYTHON_INCLUDES], [CPPFLAGS for Python, overriding output of python2.x-config --includes, e.g. "-I/opt/misc/include/python2.7"])

if test "${PYTHON_INCLUDES+set}" = set; then
  AC_MSG_NOTICE([PYTHON_INCLUDES overridden to: $PYTHON_INCLUDES])
else
  dnl deduce PYTHON_INCLUDES
  AC_MSG_CHECKING(for Python headers using $PYTHON_CONFIG --includes)
  PYTHON_INCLUDES=`$PYTHON_CONFIG --includes`
  if test $? = 0; then
    AC_MSG_RESULT($PYTHON_INCLUDES)
  else
    AC_MSG_RESULT([failed, will try another way])
    py_prefix=`$PYTHON -c "import sys; print(sys.prefix)"`
    py_exec_prefix=`$PYTHON -c "import sys; print(sys.exec_prefix)"`
    AC_MSG_CHECKING(for Python headers in $py_prefix and $py_exec_prefix)
    PYTHON_INCLUDES="-I${py_prefix}/include/python${PYTHON_VERSION}"
    if test "$py_prefix" != "$py_exec_prefix"; then
      PYTHON_INCLUDES="$PYTHON_INCLUDES -I${py_exec_prefix}/include/python${PYTHON_VERSION}"
    fi
    AC_MSG_RESULT($PYTHON_INCLUDES)
  fi
fi

AC_MSG_CHECKING(whether those headers are sufficient)
AC_SUBST(PYTHON_INCLUDES)
dnl check if the headers exist:
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $PYTHON_INCLUDES"
AC_TRY_CPP([#include <Python.h>],dnl
[AC_MSG_RESULT(found)
$1],dnl
[AC_MSG_RESULT(not found)
$2])
CPPFLAGS="$save_CPPFLAGS"
])
