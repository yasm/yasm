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


static void symrec_data_destroy(/*@only@*/ void *data);
static void symrec_data_print(void *data, FILE *f, int indent_level);

static const yasm_assoc_data_callback symrec_data_callback = {
    symrec_data_destroy,
    symrec_data_print
};

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


static int
dbg_objfmt_initialize(const char *in_filename, const char *obj_filename,
		      yasm_object *object, yasm_dbgfmt *df, yasm_arch *a)
{
    dbg_objfmt_file = fopen(obj_filename, "wt");
    if (!dbg_objfmt_file) {
	fprintf(stderr, N_("could not open file `%s'"), obj_filename);
	return 0;
    }
    fprintf(dbg_objfmt_file,
	    "initialize(\"%s\", \"%s\", %s dbgfmt, %s arch)\n",
	    in_filename, obj_filename, df->keyword, yasm_arch_keyword(a));
    return 0;
}

static void
dbg_objfmt_output(/*@unused@*/ FILE *f, yasm_object *object, int all_syms)
{
    fprintf(dbg_objfmt_file, "output(f, object->\n");
    yasm_object_print(object, dbg_objfmt_file, 1);
    fprintf(dbg_objfmt_file, "%d)\n", all_syms);
    fprintf(dbg_objfmt_file, " Symbol Table:\n");
    yasm_symtab_print(yasm_object_get_symtab(object), dbg_objfmt_file, 1);
}

static void
dbg_objfmt_cleanup(void)
{
    fprintf(dbg_objfmt_file, "cleanup()\n");
}

static /*@observer@*/ /*@null@*/ yasm_section *
dbg_objfmt_section_switch(yasm_object *object, yasm_valparamhead *valparams,
			  /*@unused@*/ /*@null@*/
			  yasm_valparamhead *objext_valparams,
			  unsigned long line)
{
    yasm_valparam *vp;
    yasm_section *retval;
    int isnew;

    fprintf(dbg_objfmt_file, "sections_switch(headp, ");
    yasm_vps_print(valparams, dbg_objfmt_file);
    fprintf(dbg_objfmt_file, ", ");
    yasm_vps_print(objext_valparams, dbg_objfmt_file);
    fprintf(dbg_objfmt_file, ", %lu), returning ", line);

    if ((vp = yasm_vps_first(valparams)) && !vp->param && vp->val != NULL) {
	retval = yasm_object_get_general(object, vp->val,
	    yasm_expr_create_ident(yasm_expr_int(yasm_intnum_create_uint(200)),
				   line), 0, &isnew, line);
	if (isnew) {
	    fprintf(dbg_objfmt_file, "(new) ");
	    yasm_symtab_define_label(yasm_object_get_symtab(object), vp->val,
				     yasm_section_bcs_first(retval), 1, line);
	}
	fprintf(dbg_objfmt_file, "\"%s\" section\n", vp->val);
	return retval;
    } else {
	fprintf(dbg_objfmt_file, "NULL\n");
	return NULL;
    }
}

static void
dbg_objfmt_extern_declare(yasm_symrec *sym, /*@unused@*/ /*@null@*/
			  yasm_valparamhead *objext_valparams,
			  unsigned long line)
{
    fprintf(dbg_objfmt_file, "extern_declare(\"%s\", ",
	    yasm_symrec_get_name(sym));
    yasm_vps_print(objext_valparams, dbg_objfmt_file);
    fprintf(dbg_objfmt_file, ", %lu)\n", line);
}

static void
dbg_objfmt_global_declare(yasm_symrec *sym, /*@unused@*/ /*@null@*/
			  yasm_valparamhead *objext_valparams,
			  unsigned long line)
{
    fprintf(dbg_objfmt_file, "global_declare(\"%s\", ",
	    yasm_symrec_get_name(sym));
    yasm_vps_print(objext_valparams, dbg_objfmt_file);
    fprintf(dbg_objfmt_file, ", %lu)\n", line);
}

static void
dbg_objfmt_common_declare(yasm_symrec *sym, /*@only@*/ yasm_expr *size,
			  /*@unused@*/ /*@null@*/
			  yasm_valparamhead *objext_valparams,
			  unsigned long line)
{
    assert(dbg_objfmt_file != NULL);
    fprintf(dbg_objfmt_file, "common_declare(\"%s\", ",
	    yasm_symrec_get_name(sym));
    yasm_expr_print(size, dbg_objfmt_file);
    fprintf(dbg_objfmt_file, ", ");
    yasm_vps_print(objext_valparams, dbg_objfmt_file);
    fprintf(dbg_objfmt_file, ", %lu), setting of_data=", line);
    yasm_expr_print(size, dbg_objfmt_file);
    yasm_symrec_add_data(sym, &symrec_data_callback, size);
    fprintf(dbg_objfmt_file, "\n");
}

static void
symrec_data_destroy(void *data)
{
    fprintf(dbg_objfmt_file, "symrec_data_delete(");
    if (data) {
	yasm_expr_print((yasm_expr *)data, dbg_objfmt_file);
	yasm_expr_destroy((yasm_expr *)data);
    }
    fprintf(dbg_objfmt_file, ")\n");
}

static void
symrec_data_print(void *data, FILE *f, int indent_level)
{
    if (data) {
	fprintf(f, "%*sSize=", indent_level, "");
	yasm_expr_print((yasm_expr *)data, f);
	fprintf(f, "\n");
    } else {
	fprintf(f, "%*s(none)\n", indent_level, "");
    }
}

static int
dbg_objfmt_directive(const char *name, yasm_valparamhead *valparams,
		     /*@null@*/ yasm_valparamhead *objext_valparams,
		     /*@unused@*/ yasm_object *object,
		     unsigned long line)
{
    fprintf(dbg_objfmt_file, "directive(\"%s\", ", name);
    yasm_vps_print(valparams, dbg_objfmt_file);
    fprintf(dbg_objfmt_file, ", ");
    yasm_vps_print(objext_valparams, dbg_objfmt_file);
    fprintf(dbg_objfmt_file, ", %lu), returning 0 (recognized)\n", line);
    return 0;	    /* dbg format "recognizes" all directives */
}


/* Define valid debug formats to use with this object format */
static const char *dbg_objfmt_dbgfmt_keywords[] = {
    "null",
    NULL
};

/* Define objfmt structure -- see objfmt.h for details */
yasm_objfmt yasm_dbg_LTX_objfmt = {
    YASM_OBJFMT_VERSION,
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
    dbg_objfmt_section_switch,
    dbg_objfmt_extern_declare,
    dbg_objfmt_global_declare,
    dbg_objfmt_common_declare,
    dbg_objfmt_directive
};
