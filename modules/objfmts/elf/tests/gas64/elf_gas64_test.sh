#! /bin/sh
# $Id: elf_amd64_test.sh 1137 2004-09-04 01:24:57Z peter $
${srcdir}/out_test.sh elf_gas64_test modules/objfmts/elf/tests/gas64 "GAS elf-amd64 objfmt" "-f elf64 -p gas" ".o"
exit $?
