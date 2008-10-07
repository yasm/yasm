#! /bin/sh
# $Id: tasm_test.sh 1137 2004-09-04 01:24:57Z peter $
${srcdir}/out_test.sh tasm_test modules/parsers/tasm/tests "tasm-compat parser" "-f bin -p tasm" ""
exit $?
