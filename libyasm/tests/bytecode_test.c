/* $IdPath$
 *
 */
#include <stdlib.h>
#include "check.h"

#include "util.h"

#include "bytecode.h"

START_TEST(test_ConvertRegToEA)
{
    effaddr static_val, *allocp, *retp;

    /* Test with static passing */
    fail_unless(ConvertRegToEA(&static_val, 1) == &static_val,
		"No allocation should be performed if non-NULL passed in ptr");
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
