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

#include "bitvect.h"

START_TEST(test_boot)
{
    fail_unless(BitVector_Boot() == ErrCode_Ok, "failed to Boot()");
}
END_TEST

typedef struct Val_s {
    const char *ascii;
    unsigned char result[10];	/* 80 bit result, little endian */
} Val;

Val oct_small_vals[] = {
    {	"0",
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
    },
    {	"1",
	{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
    },
    {	"77",
	{0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
    },
};

Val oct_large_vals[] = {
    {	"7654321076543210",
	{0x88, 0xC6, 0xFA, 0x88, 0xC6, 0xFA, 0x00, 0x00, 0x00, 0x00}
    },
    {	"12634727612534126530214",
	{0x8C, 0xB0, 0x5A, 0xE1, 0xAA, 0xF8, 0x3A, 0x67, 0x05, 0x00}
    },
    {	"61076543210",
	{0x88, 0xC6, 0xFA, 0x88, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}
    },
};

wordptr testval;

static void
num_family_setup(void)
{
    BitVector_Boot();
    testval = BitVector_Create(80, FALSE);
}

static void
num_family_teardown(void)
{
    BitVector_Destroy(testval);
}

static char result_msg[1024];

static int
num_check(Val *val)
{
    unsigned char ascii[64], *result;
    unsigned int len;
    int i;
    int ret = 0;

    strcpy((char *)ascii, val->ascii);
    strcpy(result_msg, "parser failure");
    if(BitVector_from_Oct(testval, ascii) != ErrCode_Ok)
	return 1;

    result = BitVector_Block_Read(testval, &len);

    for (i=0; i<10; i++)
	if (result[i] != val->result[i])
	    ret = 1;

    if (ret) {
	strcpy(result_msg, val->ascii);
	for (i=0; i<10; i++)
	    sprintf((char *)ascii+3*i, "%02x ", result[i]);
	strcat(result_msg, ": ");
	strcat(result_msg, (char *)ascii);
    }
    free(result);
    
    return ret;
}

START_TEST(test_oct_small_num)
{
    Val *vals = oct_small_vals;
    int i, num = sizeof(oct_small_vals)/sizeof(Val);

    for (i=0; i<num; i++)
	fail_unless(num_check(&vals[i]) == 0, result_msg);
}
END_TEST

START_TEST(test_oct_large_num)
{
    Val *vals = oct_large_vals;
    int i, num = sizeof(oct_large_vals)/sizeof(Val);

    for (i=0; i<num; i++)
	fail_unless(num_check(&vals[i]) == 0, result_msg);
}
END_TEST

static Suite *
bitvect_suite(void)
{
    Suite *s = suite_create("BitVector");
    TCase *tc_boot = tcase_create("Boot");
    TCase *tc_from_oct = tcase_create("from_Oct");

    suite_add_tcase(s, tc_boot);
    tcase_add_test(tc_boot, test_boot);

    suite_add_tcase(s, tc_from_oct);
    tcase_add_test(tc_from_oct, test_oct_small_num);
    tcase_add_test(tc_from_oct, test_oct_large_num);
    tcase_set_fixture(tc_from_oct, num_family_setup, num_family_teardown);

    return s;
}

int
main(void)
{
    int nf;
    Suite *s = bitvect_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CRNORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);
    suite_free(s);
    return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
