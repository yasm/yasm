/* $IdPath$
 *
 */
#include <stdlib.h>
#include "check.h"

#include "util.h"

#include "bytecode.h"

START_TEST(test_effaddr_new_reg)
{
    effaddr *ea;
    int i;

    /* Test with NULL */
    ea = effaddr_new_reg(1);
    fail_unless(ea != NULL, "Should die if out of memory (not return NULL)");

    /* Test structure values function should set */
    fail_unless(ea->len == 0, "len should be 0");
    fail_unless(ea->segment == 0, "Should be no segment override");
    fail_unless(ea->valid_modrm == 1, "Mod/RM should be valid");
    fail_unless(ea->need_modrm == 1, "Mod/RM should be needed");
    fail_unless(ea->valid_sib == 0, "SIB should be invalid");
    fail_unless(ea->need_sib == 0, "SIB should not be needed");

    free(ea);

    /* Exhaustively test generated Mod/RM byte with register values */
    for(i=0; i<8; i++) {
	ea = effaddr_new_reg(i);
	fail_unless(ea->modrm == 0xC0 | (i & 0x07),
		    "Invalid Mod/RM byte generated");
	free(ea);
    }
}
END_TEST

Suite *bytecode_suite(void)
{
    Suite *s = suite_create("bytecode");
    TCase *tc_conversion = tcase_create("Conversion");

    suite_add_tcase(s, tc_conversion);
    tcase_add_test(tc_conversion, test_effaddr_new_reg);

    return s;
}

int main(void)
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
