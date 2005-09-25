#! /bin/sh
# $Id$
${srcdir}/out_test.sh gas_amd64_test modules/parsers/gas/tests/amd64 "gas-compat parser" "-f elf -m amd64 -p gas" ".o"
exit $?
