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
# include <string.h>
#endif

#include <stdio.h>

#include "check.h"

#include "floatnum.c"

/* constants describing parameters of internal floating point format.
 *  (these should match those in src/floatnum.c !)
 */
#define MANT_BITS	80
#define MANT_BYTES	10

typedef struct Init_Entry_s {
    /* input ASCII value */
    const char *ascii;

    /* correct output from ASCII conversion */
    unsigned char mantissa[MANT_BYTES];	    /* little endian mantissa - first
					       byte is not checked for
					       correctness. */
    unsigned short exponent;	    /* bias 32767 exponent */
    unsigned char sign;
    unsigned char flags;

    /* correct output conversions - these should be *exact* matches */
    int ret32;
    unsigned char result32[4];
    int ret64;
    unsigned char result64[8];
    int ret80;
    unsigned char result80[10];
} Init_Entry;

/* Values used for normalized tests */
static Init_Entry normalized_vals[] = {
    {	"3.141592653589793",
	{0xc6,0x0d,0xe9,0xbd,0x68,0x21,0xa2,0xda,0x0f,0xc9},0x8000,0,0,
	 0, {0xdb,0x0f,0x49,0x40},
	 0, {0x18,0x2d,0x44,0x54,0xfb,0x21,0x09,0x40},
	 0, {0xe9,0xbd,0x68,0x21,0xa2,0xda,0x0f,0xc9,0x00,0x40}
    },
    {	"-3.141592653589793",
	{0xc6,0x0d,0xe9,0xbd,0x68,0x21,0xa2,0xda,0x0f,0xc9},0x8000,1,0,
	 0, {0xdb,0x0f,0x49,0xc0},
	 0, {0x18,0x2d,0x44,0x54,0xfb,0x21,0x09,0xc0},
	 0, {0xe9,0xbd,0x68,0x21,0xa2,0xda,0x0f,0xc9,0x00,0xc0}
    },
    {	"1.e16",
	{0x00,0x00,0x00,0x00,0x00,0x04,0xbf,0xc9,0x1b,0x8e},0x8034,0,0,
	 0, {0xca,0x1b,0x0e,0x5a},
	 0, {0x00,0x80,0xe0,0x37,0x79,0xc3,0x41,0x43},
	 0, {0x00,0x00,0x00,0x04,0xbf,0xc9,0x1b,0x8e,0x34,0x40}
    },
    {	"1.6e-20",
	{0xf6,0xd3,0xee,0x7b,0xda,0x74,0x50,0xa0,0x1d,0x97},0x7fbd,0,0,
	 0, {0xa0,0x1d,0x97,0x1e},
	 0, {0x4f,0x9b,0x0e,0x0a,0xb4,0xe3,0xd2,0x3b},
	 0, {0xef,0x7b,0xda,0x74,0x50,0xa0,0x1d,0x97,0xbd,0x3f}
    },
    {	"-5876.",
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xa0,0xb7},0x800b,1,0,
	 0, {0x00,0xa0,0xb7,0xc5},
	 0, {0x00,0x00,0x00,0x00,0x00,0xf4,0xb6,0xc0},
	 0, {0x00,0x00,0x00,0x00,0x00,0x00,0xa0,0xb7,0x0b,0xc0}
    },
};

/* Still normalized values, but edge cases of various sizes, testing underflow/
 * overflow checks as well.
 */
static Init_Entry normalized_edgecase_vals[] = {
    /* 32-bit edges */
    {	"1.1754943508222875e-38",
	{0xd5,0xf2,0x82,0xff,0xff,0xff,0xff,0xff,0xff,0xff},0x7f80,0,0,
	 0, {0x00,0x00,0x80,0x00},
	 0, {0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x38},
	 0, {0x83,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x80,0x3f}
    },
    {	"3.4028234663852886e+38",
	{0x21,0x35,0x0a,0x00,0x00,0x00,0x00,0xff,0xff,0xff},0x807e,0,0,
	 0, {0xff,0xff,0x7f,0x7f},
	 0, {0x00,0x00,0x00,0xe0,0xff,0xff,0xef,0x47},
	 0, {0x0a,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0x7e,0x40}
    },
    /* 64-bit edges */
    {	"2.2250738585072014E-308",
	{0x26,0x18,0x46,0x00,0x00,0x00,0x00,0x00,0x00,0x80},0x7c01,0,0,
	-1, {0x00,0x00,0x00,0x00},
	 0, {0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00},
	 0, {0x46,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x01,0x3c}
    },
    {	"1.7976931348623157E+308",
	{0x26,0x6b,0xac,0xf7,0xff,0xff,0xff,0xff,0xff,0xff},0x83fe,0,0,
	 1, {0x00,0x00,0x80,0x7f},
	 0, {0xff,0xff,0xff,0xff,0xff,0xff,0xef,0x7f},
	 0, {0xac,0xf7,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0x43}
    },
    /* 80-bit edges */
/*    {	"3.3621E-4932",
	{},,0,0,
	-1, {0x00,0x00,0x00,0x00},
	-1, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	 0, {}
    },
    {	"1.1897E+4932",
	{},,0,0,
	 1, {0x00,0x00,0x80,0x7f},
	 1, {},
	 0, {}
    },*/
    /* internal format edges */
/*    {
    },
    {
    },*/
};

static floatnum *flt;

/* failure messages */
static char ret_msg[1024], result_msg[1024];

static void
new_setup(Init_Entry *vals, int i)
{
    flt = floatnum_new(vals[i].ascii);
    strcpy(result_msg, vals[i].ascii);
    strcat(result_msg, ": incorrect ");
}

static int
new_check_flt(Init_Entry *val)
{
    unsigned char *mantissa;
    int i, result = 0;
    unsigned int len;

    mantissa = BitVector_Block_Read(flt->mantissa, &len);
    for (i=1;i<MANT_BYTES;i++)	    /* don't compare first byte */
	if (mantissa[i] != val->mantissa[i])
	    result = 1;
    free(mantissa);
    if (result) {
	strcat(result_msg, "mantissa");
	return 1;
    }

    if (flt->exponent != val->exponent) {
	strcat(result_msg, "exponent");
	return 1;
    }
    if (flt->sign != val->sign) {
	strcat(result_msg, "sign");
	return 1;
    }
    if (flt->flags != val->flags) {
	strcat(result_msg, "flags");
	return 1;
    }
    return 0;
}

START_TEST(test_new_normalized)
{
    Init_Entry *vals = normalized_vals;
    int i, num = sizeof(normalized_vals)/sizeof(Init_Entry);

    for (i=0; i<num; i++) {
	new_setup(vals, i);
	fail_unless(new_check_flt(&vals[i]) == 0, result_msg);
	floatnum_delete(flt);
    }
}
END_TEST

START_TEST(test_new_normalized_edgecase)
{
    Init_Entry *vals = normalized_edgecase_vals;
    int i, num = sizeof(normalized_edgecase_vals)/sizeof(Init_Entry);

    for (i=0; i<num; i++) {
	new_setup(vals, i);
	fail_unless(new_check_flt(&vals[i]) == 0, result_msg);
	floatnum_delete(flt);
    }
}
END_TEST

static void
get_family_setup(void)
{
    flt = malloc(sizeof(floatnum));
    flt->mantissa = BitVector_Create(MANT_BITS, TRUE);
}

static void
get_family_teardown(void)
{
    BitVector_Destroy(flt->mantissa);
    free(flt);
}

static void
get_common_setup(Init_Entry *vals, int i)
{
    /* set up flt */
    BitVector_Block_Store(flt->mantissa, vals[i].mantissa, MANT_BYTES);
    flt->sign = vals[i].sign;
    flt->exponent = vals[i].exponent;
    flt->flags = vals[i].flags;

    /* set failure messages */
    strcpy(ret_msg, vals[i].ascii);
    strcat(ret_msg, ": incorrect return value");
    strcpy(result_msg, vals[i].ascii);
    strcat(result_msg, ": incorrect result generated");
}
#if 0
static void
append_get_return_value(int val)
{
    char str[64];
    sprintf(str, ": %d", val);
    strcat(ret_msg, str);
}
#endif
static int
get_common_check_result(int len, const unsigned char *val,
			const unsigned char *correct)
{
    char str[64];
    int i;
    int result = 0;

    for (i=0;i<len;i++)
	if (val[i] != correct[i])
	    result = 1;

    if (result) {
	for (i=0; i<len; i++)
	    sprintf(str+3*i, "%02x ", val[i]);
	strcat(result_msg, ": ");
	strcat(result_msg, str);
    }

    return result;
}

/*
 * get_single tests
 */

START_TEST(test_get_single_normalized)
{
    unsigned char outval[4];
    Init_Entry *vals = normalized_vals;
    int i, num = sizeof(normalized_vals)/sizeof(Init_Entry);

    for (i=0; i<num; i++) {
	get_common_setup(vals, i);
	fail_unless(floatnum_get_sized(outval, flt, 4) == vals[i].ret32,
		    ret_msg);
	fail_unless(get_common_check_result(4, outval, vals[i].result32) == 0,
		    result_msg);
    }
}
END_TEST

START_TEST(test_get_single_normalized_edgecase)
{
    unsigned char outval[4];
    Init_Entry *vals = normalized_edgecase_vals;
    int i, num = sizeof(normalized_edgecase_vals)/sizeof(Init_Entry);

    for (i=0; i<num; i++) {
	get_common_setup(vals, i);
	fail_unless(floatnum_get_sized(outval, flt, 4) == vals[i].ret32,
		    ret_msg);
	fail_unless(get_common_check_result(4, outval, vals[i].result32) == 0,
		    result_msg);
    }
}
END_TEST

/*
 * get_double tests
 */

START_TEST(test_get_double_normalized)
{
    unsigned char outval[8];
    Init_Entry *vals = normalized_vals;
    int i, num = sizeof(normalized_vals)/sizeof(Init_Entry);

    for (i=0; i<num; i++) {
	get_common_setup(vals, i);
	fail_unless(floatnum_get_sized(outval, flt, 8) == vals[i].ret64,
		    ret_msg);
	fail_unless(get_common_check_result(8, outval, vals[i].result64) == 0,
		    result_msg);
    }
}
END_TEST

START_TEST(test_get_double_normalized_edgecase)
{
    unsigned char outval[8];
    Init_Entry *vals = normalized_edgecase_vals;
    int i, num = sizeof(normalized_edgecase_vals)/sizeof(Init_Entry);

    for (i=0; i<num; i++) {
	get_common_setup(vals, i);
	fail_unless(floatnum_get_sized(outval, flt, 8) == vals[i].ret64,
		    ret_msg);
	fail_unless(get_common_check_result(8, outval, vals[i].result64) == 0,
		    result_msg);
    }
}
END_TEST

/*
 * get_extended tests
 */

START_TEST(test_get_extended_normalized)
{
    unsigned char outval[10];
    Init_Entry *vals = normalized_vals;
    int i, num = sizeof(normalized_vals)/sizeof(Init_Entry);

    for (i=0; i<num; i++) {
	get_common_setup(vals, i);
	fail_unless(floatnum_get_sized(outval, flt, 10) == vals[i].ret80,
		    ret_msg);
	fail_unless(get_common_check_result(10, outval, vals[i].result80) == 0,
		    result_msg);
    }
}
END_TEST

START_TEST(test_get_extended_normalized_edgecase)
{
    unsigned char outval[10];
    Init_Entry *vals = normalized_edgecase_vals;
    int i, num = sizeof(normalized_edgecase_vals)/sizeof(Init_Entry);

    for (i=0; i<num; i++) {
	get_common_setup(vals, i);
	fail_unless(floatnum_get_sized(outval, flt, 10) == vals[i].ret80,
		    ret_msg);
	fail_unless(get_common_check_result(10, outval, vals[i].result80) == 0,
		    result_msg);
    }
}
END_TEST

static Suite *
floatnum_suite(void)
{
    Suite *s = suite_create("floatnum");
    TCase *tc_new = tcase_create("new");
    TCase *tc_get_single = tcase_create("get_single");
    TCase *tc_get_double = tcase_create("get_double");
    TCase *tc_get_extended = tcase_create("get_extended");

    suite_add_tcase(s, tc_new);
    tcase_add_test(tc_new, test_new_normalized);
    tcase_add_test(tc_new, test_new_normalized_edgecase);

    suite_add_tcase(s, tc_get_single);
    tcase_add_test(tc_get_single, test_get_single_normalized);
    tcase_add_test(tc_get_single, test_get_single_normalized_edgecase);
    tcase_set_fixture(tc_get_single, get_family_setup, get_family_teardown);

    suite_add_tcase(s, tc_get_double);
    tcase_add_test(tc_get_double, test_get_double_normalized);
    tcase_add_test(tc_get_double, test_get_double_normalized_edgecase);
    tcase_set_fixture(tc_get_double, get_family_setup, get_family_teardown);

    suite_add_tcase(s, tc_get_extended);
    tcase_add_test(tc_get_extended, test_get_extended_normalized);
    tcase_add_test(tc_get_extended, test_get_extended_normalized_edgecase);
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
