/* $IdPath$
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "util.h"

#include "check.h"

#include "bytecode.h"
#include "bc-int.h"
#include "arch.h"
#include "x86arch.h"

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
