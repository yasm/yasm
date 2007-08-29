/*
 * Invoke an external C preprocessor
 *
 *  Copyright (C) 2007       Paul Barker
 *  Copyright (C) 2001-2007  Peter Johnson
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
#include <libyasm.h>

/* TODO: Find some non-arbitrary values for these constants. */
#define TMP_BUF_SIZE 4096
#define CMDLINE_SIZE 32770

extern int isatty(int);

/* Flags. */
#define CPP_HAS_BEEN_INVOKED 0x01

typedef struct cpp_arg_entry {
    TAILQ_ENTRY(cpp_arg_entry) entry;

    char *op;
    char *param;
} cpp_arg_entry;

typedef struct yasm_preproc_cpp {
    yasm_preproc_base preproc;   /* base structure */

    TAILQ_HEAD(, cpp_arg_entry) cpp_args;
    cpp_arg_entry *pos;

    FILE *f_in, *f_out;
    yasm_linemap *cur_lm;
    yasm_errwarns *errwarns;

    int flags;
} yasm_preproc_cpp;

yasm_preproc_module yasm_cpp_LTX_preproc;

/* TODO: Make these filenames safer and more portable, maybe using tmpnam()? */
static const char *in_filename = ".cpp.in";
static const char *out_filename = ".cpp.out";

/* Invoke the c preprocessor. */
static void
cpp_invoke(yasm_preproc_cpp *pp)
{
    int r;
    char *cmdline, *p, *limit, *tmp;
    cpp_arg_entry *arg;
    size_t op_len, param_len, sz;

    /* Initialize command line. */
    cmdline = p = yasm_xmalloc(CMDLINE_SIZE);
    limit = p + CMDLINE_SIZE;
    strcpy(p, "cpp");
    p += 3;

    /* Append arguments from the list. */
    TAILQ_FOREACH(arg, (&pp->cpp_args), entry) {
        /*
            We need a space before arg->op and a space before arg->param, so
            2 extra characters.
        */
        op_len = strlen(arg->op);
        param_len = strlen(arg->param);
        sz = op_len + param_len + 2;

        if (p + sz >= limit)
            yasm__fatal("command line too long");

        *p++ = ' ';
        strcpy(p, arg->op);
        p += op_len;
        *p++ = ' ';
        strcpy(p, arg->param);
        p += param_len;

        yasm_xfree(arg->param);
    }

    /* Append final arguments. */
    r = asprintf(&tmp, " -x assembler-with-cpp -o %s %s",
                      out_filename, in_filename);
    if (r == -1)
        yasm__fatal("failed to construct command line for cpp");

    /* r is the length of the string stored in tmp. */
    if (p + r + 1 >= limit)
        yasm__fatal("command line too long");

    strcpy(p, tmp);
    yasm_xfree(tmp);
    p += r;
    *p = '\0';

#if 0
    /* Print the command line before executing. */
    printf("%s\n", cmdline);
#endif
    r = system(cmdline);
    if (r)
        yasm__fatal("C preprocessor failed");

    yasm_xfree(cmdline);
}

static yasm_preproc *
cpp_preproc_create(FILE *f, const char *in, yasm_linemap *lm,
                   yasm_errwarns *errwarns)
{
    yasm_preproc_cpp *pp = yasm_xmalloc(sizeof(yasm_preproc_cpp));

    pp->preproc.module = &yasm_cpp_LTX_preproc;
    pp->f_in = f;
    pp->f_out = NULL;
    pp->cur_lm = lm;
    pp->errwarns = errwarns;
    pp->flags = 0;

    pp->pos = NULL;
    TAILQ_INIT(&pp->cpp_args);

    return (yasm_preproc *)pp;
}

static void
cpp_preproc_destroy(yasm_preproc *preproc)
{
    yasm_preproc_cpp *pp = (yasm_preproc_cpp *)preproc;

    if (pp->f_out) {
        fclose(pp->f_out);
    }

    /* Remove both temporary files. */
    remove(in_filename);
    remove(out_filename);

    yasm_xfree(preproc);
}

static size_t
cpp_preproc_input(yasm_preproc *preproc, char *buf, size_t max_size)
{
    size_t n;
    FILE *f_tmp;
    int tty;
    char *tmp_buf;
    yasm_preproc_cpp *pp = (yasm_preproc_cpp *)preproc;

    if (! (pp->flags & CPP_HAS_BEEN_INVOKED) ) {
        pp->flags |= CPP_HAS_BEEN_INVOKED;

        tmp_buf = (char *)yasm_xmalloc(TMP_BUF_SIZE);

        f_tmp = fopen(in_filename, "w");

        tty = (isatty(fileno(f_tmp)) > 0);
        while (!feof(pp->f_in)) {
            if (tty) {
                /* TODO: Handle reading from tty into tmp_buf. */
                yasm__fatal("incapable of reading from a tty");
            } else {
                if (((n = fread(tmp_buf, 1, TMP_BUF_SIZE, pp->f_in)) == 0) &&
                        ferror(pp->f_in)) {
                    yasm_error_set(YASM_ERROR_IO,
                                   N_("error when reading from input file"));
                    yasm_errwarn_propagate(pp->errwarns,
                                          yasm_linemap_get_current(pp->cur_lm));
                }
            }

            if (fwrite(tmp_buf, 1, n, f_tmp) != n) {
                yasm__fatal("error writing to temporary file");
            }
        }

        yasm_xfree(tmp_buf);
        fclose(f_tmp);

        cpp_invoke(pp);

        pp->f_out = fopen(out_filename, "r");

        if (!pp->f_out) {
            yasm__fatal("Could not open preprocessed file \"%s\"", out_filename);
        }
    }

    /*
        Once the preprocessor has been run, we're just dealing with a normal
        file.
    */
    if (((n = fread(buf, 1, max_size, pp->f_out)) == 0) &&
               ferror(pp->f_out)) {
        yasm_error_set(YASM_ERROR_IO, N_("error when reading from preprocessed file"));
        yasm_errwarn_propagate(pp->errwarns,
                               yasm_linemap_get_current(pp->cur_lm));
    }

    return n;
}

static size_t
cpp_preproc_get_included_file(yasm_preproc *preproc, char *buf,
                              size_t max_size)
{
    /* TODO */

    return 0;
}

static void
cpp_preproc_add_include_file(yasm_preproc *preproc, const char *filename)
{
    yasm_preproc_cpp *pp = (yasm_preproc_cpp *)preproc;

    cpp_arg_entry *arg = yasm_xmalloc(sizeof(cpp_arg_entry));
    arg->op = "-include";
    arg->param = yasm__xstrdup(filename);

    TAILQ_INSERT_TAIL(&pp->cpp_args, arg, entry);
}

static void
cpp_preproc_predefine_macro(yasm_preproc *preproc, const char *macronameval)
{
    yasm_preproc_cpp *pp = (yasm_preproc_cpp *)preproc;

    cpp_arg_entry *arg = yasm_xmalloc(sizeof(cpp_arg_entry));
    arg->op = "-D";
    arg->param = yasm__xstrdup(macronameval);

    TAILQ_INSERT_TAIL(&pp->cpp_args, arg, entry);
}

static void
cpp_preproc_undefine_macro(yasm_preproc *preproc, const char *macroname)
{
    yasm_preproc_cpp *pp = (yasm_preproc_cpp *)preproc;

    cpp_arg_entry *arg = yasm_xmalloc(sizeof(cpp_arg_entry));
    arg->op = "-U";
    arg->param = yasm__xstrdup(macroname);

    TAILQ_INSERT_TAIL(&pp->cpp_args, arg, entry);
}

static void
cpp_preproc_define_builtin(yasm_preproc *preproc, const char *macronameval)
{
    /* Handle a builtin as if it were a predefine. */
    cpp_preproc_predefine_macro(preproc, macronameval);
}

yasm_preproc_module yasm_cpp_LTX_preproc = {
    "Run input through enternal C preprocessor",
    "cpp",
    cpp_preproc_create,
    cpp_preproc_destroy,
    cpp_preproc_input,
    cpp_preproc_get_included_file,
    cpp_preproc_add_include_file,
    cpp_preproc_predefine_macro,
    cpp_preproc_undefine_macro,
    cpp_preproc_define_builtin
};

/*
    TODO: We will need to pass the list of include directories to the external
            preprocessor.
*/
