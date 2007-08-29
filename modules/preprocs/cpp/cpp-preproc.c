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

/* TODO: Use autoconf to get the limit on the command line length. */
#define CMDLINE_SIZE 32770

extern int isatty(int);

/* Pre-declare the preprocessor module object. */
yasm_preproc_module yasm_cpp_LTX_preproc;

/* TODO: Use a pipe rather than a temporary file? */
static const char *out_filename = ".cpp.out";

/*******************************************************************************
    Structures.
*******************************************************************************/

/* An entry in a list of arguments to pass to cpp. */
typedef struct cpp_arg_entry {
    TAILQ_ENTRY(cpp_arg_entry) entry;

    /*
        The operator (eg "-I") and the parameter (eg "include/"). op is expected
        to point to a string literal, whereas param is expected to be a copy of
        the parameter which is free'd when no-longer needed (in
        cpp_build_cmdline()).
    */
    const char *op;
    char *param;
} cpp_arg_entry;

typedef struct yasm_preproc_cpp {
    yasm_preproc_base preproc;   /* base structure */

    /*
        List of arguments to pass to cpp.

        TODO: We should properly destroy this list and free all memory
        associated with it once we have finished with it (ie. at the end of
        cpp_build_cmdline()).
    */
    TAILQ_HEAD(, cpp_arg_entry) cpp_args;

    char *filename;
    FILE *f;
    yasm_linemap *cur_lm;
    yasm_errwarns *errwarns;

    int flags;
} yasm_preproc_cpp;

/* Flag values for yasm_preproc_cpp->flags. */
#define CPP_HAS_BEEN_INVOKED 0x01

/*******************************************************************************
    Internal functions and helpers.
*******************************************************************************/

/*
    Append a string to the command line, ensuring that we don't overflow the
    buffer.
*/
#define APPEND(s) do {                              \
    size_t _len = strlen(s);                        \
    if (p + _len >= limit)                          \
        yasm__fatal("command line too long!");      \
    strcpy(p, s);                                   \
    p += _len;                                      \
} while (0)

/*
    Put all the options together into a command line that can be used to invoke
    cpp.
*/
static char *
cpp_build_cmdline(yasm_preproc_cpp *pp)
{
    char *cmdline, *p, *limit;
    cpp_arg_entry *arg;

    /*
        Initialize command line. We can assume there will be enough space to
        store "cpp".
    */
    cmdline = p = yasm_xmalloc(CMDLINE_SIZE);
    limit = p + CMDLINE_SIZE;
    strcpy(p, "cpp");
    p += 3;

    /* Append arguments from the list. */
    TAILQ_FOREACH(arg, (&pp->cpp_args), entry) {
        APPEND(" ");
        APPEND(arg->op);
        APPEND(" ");
        APPEND(arg->param);

        yasm_xfree(arg->param);
    }

    /* Append final arguments. */
    APPEND(" -x assembler-with-cpp -o ");
    APPEND(out_filename);
    APPEND(" ");
    APPEND(pp->filename);

    return cmdline;
}

/* Invoke the c preprocessor. */
static void
cpp_invoke(yasm_preproc_cpp *pp)
{
    int r;
    char *cmdline;

    cmdline = cpp_build_cmdline(pp);

#if 0
    /* Print the command line before executing. */
    printf("%s\n", cmdline);
#endif

    r = system(cmdline);
    if (r)
        yasm__fatal("C preprocessor failed");

    yasm_xfree(cmdline);

    /* Open the preprocessed file. */
    pp->f = fopen(out_filename, "r");

    if (!pp->f) {
        yasm__fatal("Could not open preprocessed file \"%s\"", out_filename);
    }
}

/*******************************************************************************
    Interface functions.
*******************************************************************************/
static yasm_preproc *
cpp_preproc_create(FILE *f, const char *in, yasm_linemap *lm,
                   yasm_errwarns *errwarns)
{
    yasm_preproc_cpp *pp = yasm_xmalloc(sizeof(yasm_preproc_cpp));

    pp->preproc.module = &yasm_cpp_LTX_preproc;
    pp->f = NULL;
    pp->cur_lm = lm;
    pp->errwarns = errwarns;
    pp->flags = 0;
    pp->filename = yasm__xstrdup(in);

    TAILQ_INIT(&pp->cpp_args);

    /* We can't handle reading from a tty yet. */
    if (isatty(fileno(f)) > 0)
        yasm__fatal("incapable of reading from a tty");

    /*
        We don't need the FILE* given by yasm since we will simply pass the
        filename to cpp, but closing it causes a segfault.

        TODO: Change the preprocessor interface, so no FILE* is given.
    */

    return (yasm_preproc *)pp;
}

static void
cpp_preproc_destroy(yasm_preproc *preproc)
{
    yasm_preproc_cpp *pp = (yasm_preproc_cpp *)preproc;

    if (pp->f) {
        fclose(pp->f);
    }

    /* Remove temporary file. */
    remove(out_filename);

    yasm_xfree(pp->filename);
    yasm_xfree(pp);
}

static size_t
cpp_preproc_input(yasm_preproc *preproc, char *buf, size_t max_size)
{
    size_t n;
    yasm_preproc_cpp *pp = (yasm_preproc_cpp *)preproc;

    if (! (pp->flags & CPP_HAS_BEEN_INVOKED) ) {
        pp->flags |= CPP_HAS_BEEN_INVOKED;

        cpp_invoke(pp);
    }

    /*
        Once the preprocessor has been run, we're just dealing with a normal
        file.
    */
    if (((n = fread(buf, 1, max_size, pp->f)) == 0) &&
               ferror(pp->f)) {
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

/*******************************************************************************
    Preprocessor module object.
*******************************************************************************/

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
