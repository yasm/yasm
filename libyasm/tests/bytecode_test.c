/* $IdPath$
 *
 */
#include <stdlib.h>
#include "check.h"

#include "util.h"

#include "bytecode.h"

START_TEST(test_ConvertRegToEA)
{
    effaddr static_val, *retp;
    int i;

    /* Test with non-NULL */
    fail_unless(ConvertRegToEA(&static_val, 1) == &static_val,
		"Should return ptr if non-NULL passed in ptr");

    /* Test with NULL */
    retp = ConvertRegToEA(NULL, 1);
    fail_unless(retp != NULL,
		"Should return static structure if NULL passed in ptr");

    /* Test structure values function should set */
    fail_unless(retp->len == 0, "len should be 0");
    fail_unless(retp->segment == 0, "Should be no segment override");
    fail_unless(retp->valid_modrm == 1, "Mod/RM should be valid");
    fail_unless(retp->need_modrm == 1, "Mod/RM should be needed");
    fail_unless(retp->valid_sib == 0, "SIB should be invalid");
    fail_unless(retp->need_sib == 0, "SIB should not be needed");

    /* Exhaustively test generated Mod/RM byte with register values */
    for(i=0; i<8; i++) {
	ConvertRegToEA(&static_val, i);
	fail_unless(static_val.modrm == 0xC0 | (i & 0x07),
		    "Invalid Mod/RM byte generated");
    }
}
END_TEST

Suite *bytecode_suite(void)
{
    Suite *s = suite_create("bytecode");
    TCase *tc_conversion = tcase_create("Conversion");

    suite_add_tcase(s, tc_conversion);
    tcase_add_test(tc_conversion, test_ConvertRegToEA);

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
