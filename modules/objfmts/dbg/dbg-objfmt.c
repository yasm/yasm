/*
 * Debugging object format (used to debug object format module interface)
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
#include <util.h>
/*@unused@*/ RCSID("$IdPath$");

#define YASM_LIB_INTERNAL
#include <libyasm.h>


yasm_objfmt yasm_dbg_LTX_objfmt;

/* Output file for debugging-type formats.  This is so that functions that are
 * called before the object file is usually opened can still write data out to
 * it (whereas for "normal" formats the object file is not opened until later
 * in the assembly process).  Opening the file early is done in initialize().
 *
 * Note that the functions here write to dbg_objfmt_file.  This is NOT legal
 * for other object formats to do--only the output() function can write to a
 * file, and only the file it's passed in its f parameter!
 */
static FILE *dbg_objfmt_file;


static void
dbg_objfmt_initialize(const char *in_filename, const char *obj_filename,
		      yasm_dbgfmt *df, yasm_arch *a)
{
    dbg_objfmt_file = fopen(obj_filename, "wt");
    if (!dbg_objfmt_file) {
	fprintf(stderr, N_("could not open file `%s'"), obj_filename);
	return;
    }
    fprintf(dbg_objfmt_file,
	    "initialize(\"%s\", \"%s\", %s dbgfmt, %s arch)\n",
	    in_filename, obj_filename, df->keyword, a->keyword);
}

static void
dbg_objfmt_output(/*@unused@*/ FILE *f, yasm_sectionhead *sections,
		  int all_syms)
{
    fprintf(dbg_objfmt_file, "output(f, sections->\n");
    yasm_sections_print(dbg_objfmt_file, 1, sections);
    fprintf(dbg_objfmt_file, "%d)\n", all_syms);
    fprintf(dbg_objfmt_file, " Symbol Table:\n");
    yasm_symrec_print_all(dbg_objfmt_file, 1);
}

static void
dbg_objfmt_cleanup(void)
{
    fprintf(dbg_objfmt_file, "cleanup()\n");
}

static /*@observer@*/ /*@null@*/ yasm_section *
dbg_objfmt_sections_switch(yasm_sectionhead *headp,
			   yasm_valparamhead *valparams,
			   /*@unused@*/ /*@null@*/
			   yasm_valparamhead *objext_valparams,
			   unsigned long lindex)
{
    yasm_valparam *vp;
    yasm_section *retval;
    int isnew;

    fprintf(dbg_objfmt_file, "sections_switch(headp, ");
    yasm_vps_print(dbg_objfmt_file, valparams);
    fprintf(dbg_objfmt_file, ", ");
    yasm_vps_print(dbg_objfmt_file, objext_valparams);
    fprintf(dbg_objfmt_file, ", %lu), returning ", lindex);

    if ((vp = yasm_vps_first(valparams)) && !vp->param && vp->val != NULL) {
	retval = yasm_sections_switch_general(headp, vp->val,
	    yasm_expr_new_ident(yasm_expr_int(yasm_intnum_new_uint(200)),
				lindex), 0, &isnew, lindex);
	if (isnew) {
	    fprintf(dbg_objfmt_file, "(new) ");
	    yasm_symrec_define_label(vp->val, retval, (yasm_bytecode *)NULL, 1,
				     lindex);
	}
	fprintf(dbg_objfmt_file, "\"%s\" section\n", vp->val);
	return retval;
    } else {
	fprintf(dbg_objfmt_file, "NULL\n");
	return NULL;
    }
}

static void
dbg_objfmt_section_data_delete(/*@only@*/ void *data)
{
    fprintf(dbg_objfmt_file, "section_data_delete(%p)\n", data);
    yasm_xfree(data);
}

static void
dbg_objfmt_section_data_print(FILE *f, int indent_level, /*@null@*/ void *data)
{
    if (data)
	fprintf(f, "%*s%p\n", indent_level, "", data);
    else
	fprintf(f, "%*s(none)\n", indent_level, "");
}

static void
dbg_objfmt_extern_declare(yasm_symrec *sym, /*@unused@*/ /*@null@*/
			  yasm_valparamhead *objext_valparams,
			  unsigned long lindex)
{
    fprintf(dbg_objfmt_file, "extern_declare(\"%s\", ",
	    yasm_symrec_get_name(sym));
    yasm_vps_print(dbg_objfmt_file, objext_valparams);
    fprintf(dbg_objfmt_file, ", %lu), setting of_data=NULL\n", lindex);
    yasm_symrec_set_of_data(sym, &yasm_dbg_LTX_objfmt, NULL);
}

static void
dbg_objfmt_global_declare(yasm_symrec *sym, /*@unused@*/ /*@null@*/
			  yasm_valparamhead *objext_valparams,
			  unsigned long lindex)
{
    fprintf(dbg_objfmt_file, "global_declare(\"%s\", ",
	    yasm_symrec_get_name(sym));
    yasm_vps_print(dbg_objfmt_file, objext_valparams);
    fprintf(dbg_objfmt_file, ", %lu), setting of_data=NULL\n", lindex);
    yasm_symrec_set_of_data(sym, &yasm_dbg_LTX_objfmt, NULL);
}

static void
dbg_objfmt_common_declare(yasm_symrec *sym, /*@only@*/ yasm_expr *size,
			  /*@unused@*/ /*@null@*/
			  yasm_valparamhead *objext_valparams,
			  unsigned long lindex)
{
    assert(dbg_objfmt_file != NULL);
    fprintf(dbg_objfmt_file, "common_declare(\"%s\", ",
	    yasm_symrec_get_name(sym));
    yasm_expr_print(dbg_objfmt_file, size);
    fprintf(dbg_objfmt_file, ", ");
    yasm_vps_print(dbg_objfmt_file, objext_valparams);
    fprintf(dbg_objfmt_file, ", %lu), setting of_data=", lindex);
    yasm_expr_print(dbg_objfmt_file, size);
    yasm_symrec_set_of_data(sym, &yasm_dbg_LTX_objfmt, size);
    fprintf(dbg_objfmt_file, "\n");
}

static void
dbg_objfmt_symrec_data_delete(/*@only@*/ void *data)
{
    fprintf(dbg_objfmt_file, "symrec_data_delete(");
    if (data) {
	yasm_expr_print(dbg_objfmt_file, data);
	yasm_expr_delete(data);
    }
    fprintf(dbg_objfmt_file, ")\n");
}

static void
dbg_objfmt_symrec_data_print(FILE *f, int indent_level, /*@null@*/ void *data)
{
    if (data) {
	fprintf(f, "%*sSize=", indent_level, "");
	yasm_expr_print(f, data);
	fprintf(f, "\n");
    } else {
	fprintf(f, "%*s(none)\n", indent_level, "");
    }
}

static int
dbg_objfmt_directive(const char *name, yasm_valparamhead *valparams,
		     /*@null@*/ yasm_valparamhead *objext_valparams,
		     /*@unused@*/ yasm_sectionhead *headp,
		     unsigned long lindex)
{
    fprintf(dbg_objfmt_file, "directive(\"%s\", ", name);
    yasm_vps_print(dbg_objfmt_file, valparams);
    fprintf(dbg_objfmt_file, ", ");
    yasm_vps_print(dbg_objfmt_file, objext_valparams);
    fprintf(dbg_objfmt_file, ", %lu), returning 0 (recognized)\n", lindex);
    return 0;	    /* dbg format "recognizes" all directives */
}

static void
dbg_objfmt_bc_objfmt_data_delete(unsigned int type, /*@only@*/ void *data)
{
    fprintf(dbg_objfmt_file, "symrec_data_delete(%u, %p)\n", type, data);
    yasm_xfree(data);
}

static void
dbg_objfmt_bc_objfmt_data_print(FILE *f, int indent_level, unsigned int type,
				const void *data)
{
    fprintf(f, "%*sType=%u\n", indent_level, "", type);
    fprintf(f, "%*sData=%p\n", indent_level, "", data);
}


/* Define valid debug formats to use with this object format */
static const char *dbg_objfmt_dbgfmt_keywords[] = {
    "null",
    NULL
};

/* Define objfmt structure -- see objfmt.h for details */
yasm_objfmt yasm_dbg_LTX_objfmt = {
    "Trace of all info passed to object format module",
    "dbg",
    "dbg",
    ".text",
    32,
    dbg_objfmt_dbgfmt_keywords,
    "null",
    dbg_objfmt_initialize,
    dbg_objfmt_output,
    dbg_objfmt_cleanup,
    dbg_objfmt_sections_switch,
    dbg_objfmt_section_data_delete,
    dbg_objfmt_section_data_print,
    dbg_objfmt_extern_declare,
    dbg_objfmt_global_declare,
    dbg_objfmt_common_declare,
    dbg_objfmt_symrec_data_delete,
    dbg_objfmt_symrec_data_print,
    dbg_objfmt_directive,
    dbg_objfmt_bc_objfmt_data_delete,
    dbg_objfmt_bc_objfmt_data_print
};
