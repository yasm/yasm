/* $IdPath$
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 *  This file is part of YASM.
 *
 *  YASM is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  YASM is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "util.h"

#include "check.h"

#include "bytecode.h"
#include "bc-int.h"
#include "arch.h"
#include "x86-int.h"

START_TEST(test_x86_ea_new_reg)
{
    effaddr *ea;
    x86_effaddr_data *ead;
    int i;

    /* Test with NULL */
    ea = x86_ea_new_reg(1);
    fail_unless(ea != NULL, "Should die if out of memory (not return NULL)");

    /* Test structure values function should set */
    fail_unless(ea->len == 0, "len should be 0");
    ead = ea_get_data(ea);
    fail_unless(ead->segment == 0, "Should be no segment override");
    fail_unless(ead->valid_modrm == 1, "Mod/RM should be valid");
    fail_unless(ead->need_modrm == 1, "Mod/RM should be needed");
    fail_unless(ead->valid_sib == 0, "SIB should be invalid");
    fail_unless(ead->need_sib == 0, "SIB should not be needed");

    free(ea);

    /* Exhaustively test generated Mod/RM byte with register values */
    for(i=0; i<8; i++) {
	ea = x86_ea_new_reg(i);
	ead = ea_get_data(ea);
	fail_unless(ead->modrm == (0xC0 | (i & 0x07)),
		    "Invalid Mod/RM byte generated");
	free(ea);
    }
}
END_TEST

static Suite *
bytecode_suite(void)
{
    Suite *s = suite_create("bytecode");
    TCase *tc_conversion = tcase_create("Conversion");

    suite_add_tcase(s, tc_conversion);
    tcase_add_test(tc_conversion, test_x86_ea_new_reg);

    return s;
}

int
main(void)
{
    int nf;
    Suite *s = bytecode_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CRNORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);
    suite_free(s);
    return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
