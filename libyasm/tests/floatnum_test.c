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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef STDC_HEADERS
# include <stdlib.h>
#endif

#include "check.h"

#include "bitvect.h"

#include "floatnum.h"

floatnum *flt;
static void
get_family_setup(void)
{

    flt = malloc(sizeof(floatnum));
    flt->mantissa = BitVector_Create(80, TRUE);
}

static void
get_family_teardown(void)
{
    BitVector_Destroy(flt->mantissa);
    free(flt);
}

static void
pi_setup(void)
{
    /* test value: 3.141592653589793 */
    /* 80-bit little endian mantissa: C6 0D E9 BD 68 21 A2 DA 0F C9 */
    unsigned char val[] = {0xC6, 0x0D, 0xE9, 0xBD, 0x68, 0x21, 0xA2, 0xDA, 0x0F, 0xC9};
    unsigned char sign = 0;
    unsigned short exp = 32767 + 1;

    BitVector_Block_Store(flt->mantissa, val, 10);
    flt->sign = sign;
    flt->exponent = exp;
}

START_TEST(test_get_single_pi)
{
    unsigned char outval[] = {0x00, 0x00, 0x00, 0x00};
    unsigned char correct[] = {0xDB, 0x0F, 0x49, 0x40};
    int i;

    pi_setup();

    fail_unless(floatnum_get_single(outval, flt) == 0,
		"Should not fail on this value");

    /* Compare result with what we should get. */
    for (i=0;i<4;i++)
	fail_unless(outval[i] == correct[i], "Invalid result generated");
}
END_TEST

START_TEST(test_get_double_pi)
{
    unsigned char outval[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char correct[] = {0x18, 0x2D, 0x44, 0x54, 0xFB, 0x21, 0x09, 0x40};
    int i;

    pi_setup();

    fail_unless(floatnum_get_double(outval, flt) == 0,
		"Should not fail on this value");

    /* Compare result with what we should get. */
    for (i=0;i<8;i++)
	fail_unless(outval[i] == correct[i], "Invalid result generated");
}
END_TEST

START_TEST(test_get_extended_pi)
{
    unsigned char outval[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char correct[] = {0xE9, 0xBD, 0x68, 0x21, 0xA2, 0xDA, 0x0F, 0xC9, 0x00, 0x40};
    int i;

    pi_setup();

    fail_unless(floatnum_get_extended(outval, flt) == 0,
		"Should not fail on this value");

    /* Compare result with what we should get. */
    for (i=0;i<10;i++)
	fail_unless(outval[i] == correct[i], "Invalid result generated");
}
END_TEST

static Suite *
floatnum_suite(void)
{
    Suite *s = suite_create("floatnum");
    TCase *tc_get_single = tcase_create("get_single");
    TCase *tc_get_double = tcase_create("get_double");
    TCase *tc_get_extended = tcase_create("get_extended");

    suite_add_tcase(s, tc_get_single);
    tcase_add_test(tc_get_single, test_get_single_pi);
    tcase_set_fixture(tc_get_single, get_family_setup, get_family_teardown);

    suite_add_tcase(s, tc_get_double);
    tcase_add_test(tc_get_double, test_get_double_pi);
    tcase_set_fixture(tc_get_double, get_family_setup, get_family_teardown);

    suite_add_tcase(s, tc_get_extended);
    tcase_add_test(tc_get_extended, test_get_extended_pi);
    tcase_set_fixture(tc_get_extended, get_family_setup, get_family_teardown);

    return s;
}

int
main(void)
{
    int nf;
    Suite *s = floatnum_suite();
    SRunner *sr = srunner_create(s);
    BitVector_Boot();
    srunner_run_all(sr, CRNORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);
    suite_free(s);
    return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
