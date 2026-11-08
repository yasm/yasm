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
#ifdef _WIN32
#include <process.h>
#endif

/* TODO: Use autoconf to get the limit on the command line length. */
#define CMDLINE_SIZE 32770

#define BSIZE 512

#define CL_EXE "cl.exe"

#if !defined(HAVE_POPEN) && defined(_WIN32)
#define popen _popen
#define pclose _pclose
#define HAVE_POPEN
#endif

/* Pre-declare the preprocessor module object. */
yasm_preproc_module yasm_cpp_LTX_preproc;

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
        cpp_preproc_destroy()).
    */
    const char *op;
    char *param;
} cpp_arg_entry;

/* A file that said "#pragma once", inlined into cl's input already. */
typedef struct cpp_once_entry {
    struct cpp_once_entry *next;
    char *path;
} cpp_once_entry;

typedef struct yasm_preproc_cpp {
    yasm_preproc_base preproc;   /* base structure */

    /* List of arguments to pass to cpp. */
    TAILQ_HEAD(cpp_arg_head, cpp_arg_entry) cpp_args;

    char *filename;
    FILE *f, *f_deps;
    yasm_linemap *cur_lm;
    yasm_errwarns *errwarns;

    int flags;

    /* The temporary file cl reads instead of filename, and the "#pragma once" files inlined into it. */
    char *ms_input;
    cpp_once_entry *ms_once;
} yasm_preproc_cpp;

/* Flag values for yasm_preproc_cpp->flags. */
#define CPP_HAS_BEEN_INVOKED        0x01
#define CPP_HAS_GENERATED_DEPS      0x02
#define CPP_HAS_MS_CPP              0x04 /* use MS compiler for cpp */

#define CPP_MAX_INCLUDE_DEPTH       64

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
        yasm__fatal(N_("command line too long!"));  \
    strcpy(p, s);                                   \
    p += _len;                                      \
} while (0)

static int
cpp_is_ms_cl(void)
{
    size_t cpp_len = strlen(CPP_PROG), cl_len = strlen(CL_EXE);
    int cpp_prog_quoted;

    if (cpp_len == 0)
        return 0;
    cpp_prog_quoted = (CPP_PROG[cpp_len-1] == '\"' || CPP_PROG[cpp_len-1] == '\'') ? 1 : 0;
    return !yasm__strcasecmp(CPP_PROG, "cl") ||
        (cpp_len >= cl_len + cpp_prog_quoted &&
         !yasm__strncasecmp(CPP_PROG + cpp_len - cl_len - cpp_prog_quoted, CL_EXE, cl_len));
}

/* Reads a whole line of any length into *buf, growing it; returns 0 at the end of the file. */
static int
cpp_read_line(FILE *f, char **buf, size_t *size)
{
    size_t len = 0;

    for (;;) {
        if (!fgets(*buf + len, (int)(*size - len), f))
            return len > 0;
        len += strlen(*buf + len);
        if (len > 0 && (*buf)[len-1] == '\n')
            return 1;
        if (len + 1 >= *size) {
            *size *= 2;
            *buf = yasm_xrealloc(*buf, *size);
        }
    }
}

static int
cpp_is_ident_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
        c == '$';
}

/*
    cl allows $ in names, so it reads "$C1" as one name and never substitutes a macro argument written right after
    $; a $ that starts a name gets a space after it, which GAS reads the same way.
*/
static void
cpp_ms_put_line(FILE *out, const char *line)
{
    const char *p;
    int in_string = 0;

    for (p = line; *p; p++) {
        fputc(*p, out);
        if (in_string && *p == '\\' && p[1]) {
            fputc(*++p, out);
            continue;
        }
        if (*p == '"')
            in_string = !in_string;
        else if (*p == '$' && !in_string && (p == line || !cpp_is_ident_char(p[-1])) &&
                 ((p[1] >= 'a' && p[1] <= 'z') || (p[1] >= 'A' && p[1] <= 'Z') || p[1] == '_'))
            fputc(' ', out);
    }
    if (p == line || p[-1] != '\n')
        fputc('\n', out);
}

static void
cpp_ms_line_marker(FILE *out, unsigned long line, const char *path)
{
    fprintf(out, "#line %lu \"", line);
    for (; *path; path++) {
        if (*path == '\\')
            fputc('\\', out);
        fputc(*path, out);
    }
    fputs("\"\n", out);
}

/* The name in an #include "name" or #include <name> line, or NULL; *angle tells which form. */
static char *
cpp_include_name(const char *line, int *angle)
{
    const char *p = line, *end;
    char close;

    while (*p == ' ' || *p == '\t')
        p++;
    if (*p++ != '#')
        return NULL;
    while (*p == ' ' || *p == '\t')
        p++;
    if (strncmp(p, "include", 7) != 0)
        return NULL;
    p += 7;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '"')
        close = '"';
    else if (*p == '<')
        close = '>';
    else
        return NULL;
    end = strchr(p + 1, close);
    if (!end)
        return NULL;
    *angle = close == '>';
    return yasm__xstrndup(p + 1, (size_t)(end - p - 1));
}

static int
cpp_is_pragma_once(const char *line)
{
    const char *p = line;

    while (*p == ' ' || *p == '\t')
        p++;
    if (*p++ != '#')
        return 0;
    while (*p == ' ' || *p == '\t')
        p++;
    if (strncmp(p, "pragma", 6) != 0 || (p[6] != ' ' && p[6] != '\t'))
        return 0;
    p += 6;
    while (*p == ' ' || *p == '\t')
        p++;
    return strncmp(p, "once", 4) == 0 && strspn(p + 4, " \t\r\n") == strlen(p + 4);
}

static char *
cpp_join_path(const char *dir, size_t dir_len, const char *name)
{
    char *path;

    if (name[0] == '/' || name[0] == '\\' || (name[0] && name[1] == ':') || dir_len == 0)
        return yasm__xstrdup(name);
    path = yasm_xmalloc(dir_len + strlen(name) + 2);
    memcpy(path, dir, dir_len);
    if (dir[dir_len-1] != '/' && dir[dir_len-1] != '\\')
        path[dir_len++] = '/';
    strcpy(path + dir_len, name);
    return path;
}

/* Opens an included file as cl would find it: a quoted name next to the including file first, then the -I dirs. */
static FILE *
cpp_ms_open_include(yasm_preproc_cpp *pp, const char *from, const char *name, int angle, char **path)
{
    const char *slash = strrchr(from, '/'), *bslash = strrchr(from, '\\');
    cpp_arg_entry *arg;
    FILE *f;

    if (bslash && (!slash || bslash > slash))
        slash = bslash;
    if (!angle) {
        *path = cpp_join_path(from, slash ? (size_t)(slash - from) : 0, name);
        if ((f = fopen(*path, "r")) != NULL)
            return f;
        yasm_xfree(*path);
    }
    for (arg = TAILQ_FIRST(&pp->cpp_args); arg; arg = TAILQ_NEXT(arg, entry)) {
        if (strcmp(arg->op, "-I") != 0)
            continue;
        *path = cpp_join_path(arg->param, strlen(arg->param), name);
        if ((f = fopen(*path, "r")) != NULL)
            return f;
        yasm_xfree(*path);
    }
    *path = NULL;
    return NULL;
}

static int
cpp_ms_seen_once(yasm_preproc_cpp *pp, const char *path)
{
    cpp_once_entry *once;

    for (once = pp->ms_once; once; once = once->next) {
        if (strcmp(once->path, path) == 0)
            return 1;
    }
    return 0;
}

/*
    Writes the file to out with every header it includes inlined in its place, so that cpp_ms_put_line reaches the
    macros defined in headers too; #line markers keep cl's file names and line numbers those of the real files. An
    include that cannot be found here stays a directive for cl to resolve.
*/
static void
cpp_ms_flatten(yasm_preproc_cpp *pp, FILE *in, const char *path, FILE *out, int depth)
{
    size_t size = BSIZE;
    char *buf = yasm_xmalloc(size);
    unsigned long line = 0;

    cpp_ms_line_marker(out, 1, path);
    while (cpp_read_line(in, &buf, &size)) {
        int angle = 0;
        char *name = cpp_include_name(buf, &angle);
        char *inc_path = NULL;
        FILE *inc = NULL;

        line++;
        if (name && depth < CPP_MAX_INCLUDE_DEPTH)
            inc = cpp_ms_open_include(pp, path, name, angle, &inc_path);
        if (inc) {
            if (!cpp_ms_seen_once(pp, inc_path))
                cpp_ms_flatten(pp, inc, inc_path, out, depth + 1);
            fclose(inc);
            yasm_xfree(inc_path);
            cpp_ms_line_marker(out, line + 1, path);
        } else {
            if (cpp_is_pragma_once(buf) && !cpp_ms_seen_once(pp, path)) {
                cpp_once_entry *once = yasm_xmalloc(sizeof(cpp_once_entry));
                once->path = yasm__xstrdup(path);
                once->next = pp->ms_once;
                pp->ms_once = once;
            }
            cpp_ms_put_line(out, buf);
        }
        if (name)
            yasm_xfree(name);
    }
    yasm_xfree(buf);
}

/*
    Writes the input, flattened, to a new file in the temporary directory and returns its path. The name holds the
    process id and the file is created only if it does not exist yet, so parallel builds never share one.
*/
static char *
cpp_ms_prepare_input(yasm_preproc_cpp *pp)
{
    const char *ext = strrchr(pp->filename, '.');
    const char *dir = getenv("TMP");
    char *path;
    FILE *in, *out = NULL;
    unsigned int n;

    if (!dir || !*dir)
        dir = getenv("TEMP");
    if (!dir || !*dir)
        dir = ".";
    if (!ext || strchr(ext, '/') || strchr(ext, '\\'))
        ext = "";
    path = yasm_xmalloc(strlen(dir) + strlen(ext) + 48);
#ifdef _WIN32
    for (n = 0; n < 1000 && !out; n++) {
        sprintf(path, "%s\\yasm-%d-%u%s", dir, _getpid(), n, ext);
        out = fopen(path, "wx");
    }
#endif
    if (!out)
        yasm__fatal(N_("could not create a temporary file for the preprocessor"));

    in = fopen(pp->filename, "r");
    if (!in)
        yasm__fatal(N_("could not open file `%s'"), pp->filename);
    cpp_ms_flatten(pp, in, pp->filename, out, 0);
    fclose(in);
    if (fclose(out) != 0)
        yasm__fatal(N_("could not write file `%s'"), path);
    return path;
}

/*
    Put all the options together into a command line that can be used to invoke
    cpp.
*/
static char *
cpp_build_cmdline(yasm_preproc_cpp *pp, const char *extra, const char *input)
{
    char *cmdline, *p, *limit;
    cpp_arg_entry *arg;
    size_t cpp_len = strlen(CPP_PROG);

    /* Initialize command line. */
    cmdline = p = yasm_xmalloc(cpp_len +CMDLINE_SIZE);
    limit = p + CMDLINE_SIZE;
    /* popen runs cmd /c, which drops the first and the last quote of a line that starts with one */
    if (pp->flags & CPP_HAS_MS_CPP)
        *p++ = '"';
    strcpy(p, CPP_PROG);
    p += cpp_len;

    arg = TAILQ_FIRST(&pp->cpp_args);

    /* Append arguments from the list. */
    while ( arg ) {
        APPEND(" ");
        APPEND(arg->op);
        APPEND(" ");
        APPEND(arg->param);

        arg = TAILQ_NEXT(arg, entry);
    }

    /* Append extra arguments. */
    if (extra) {
        APPEND(" ");
        APPEND(extra);
    }
    /* Append final arguments. */
    if (pp->flags & CPP_HAS_MS_CPP) {
        APPEND(" /nologo /E \"");
        APPEND(input);
        APPEND("\"\"");
    } else {
        APPEND(" -x assembler-with-cpp ");
        APPEND(input);
    }

    return cmdline;
}

/* Invoke the c preprocessor. */
static void
cpp_invoke(yasm_preproc_cpp *pp)
{
    char *cmdline;

    if (pp->flags & CPP_HAS_MS_CPP) {
        pp->ms_input = cpp_ms_prepare_input(pp);
        cmdline = cpp_build_cmdline(pp, NULL, pp->ms_input);
    } else
        cmdline = cpp_build_cmdline(pp, NULL, pp->filename);

#ifdef HAVE_POPEN
    pp->f = popen(cmdline, "r");
    if (!pp->f)
        yasm__fatal( N_("Failed to execute preprocessor") );
#else
    yasm__fatal( N_("Cannot execute preprocessor, no popen available") );
#endif

    yasm_xfree(cmdline);
}

/* Free memory used by the list of arguments. */
static void
cpp_destroy_args(yasm_preproc_cpp *pp)
{
    cpp_arg_entry *arg;

    while ( (arg = TAILQ_FIRST(&pp->cpp_args)) ) {
        TAILQ_REMOVE(&pp->cpp_args, arg, entry);
        yasm_xfree(arg->param);
        yasm_xfree(arg);
    }
}

/* Invoke the c preprocessor to generate dependency info. */
static void
cpp_generate_deps(yasm_preproc_cpp *pp)
{
    char *cmdline;

    cmdline = cpp_build_cmdline(pp, "-M", pp->filename);

#ifdef HAVE_POPEN
    pp->f_deps = popen(cmdline, "r");
    if (!pp->f_deps)
        yasm__fatal( N_("Failed to execute preprocessor") );
#else
    yasm__fatal( N_("Cannot execute preprocessor, no popen available") );
#endif

    yasm_xfree(cmdline);
}

/*******************************************************************************
    Interface functions.
*******************************************************************************/
static yasm_preproc *
cpp_preproc_create(const char *in, yasm_symtab *symtab, yasm_linemap *lm,
                   yasm_errwarns *errwarns)
{
    yasm_preproc_cpp *pp = yasm_xmalloc(sizeof(yasm_preproc_cpp));
    void * iter;
    const char * inc_dir;

    pp->preproc.module = &yasm_cpp_LTX_preproc;
    pp->f = pp->f_deps = NULL;
    pp->cur_lm = lm;
    pp->errwarns = errwarns;
    pp->flags = cpp_is_ms_cl() ? CPP_HAS_MS_CPP : 0;
    pp->filename = yasm__xstrdup(in);
    pp->ms_input = NULL;
    pp->ms_once = NULL;

    TAILQ_INIT(&pp->cpp_args);

    /* Iterate through the list of include dirs. */
    iter = NULL;
    while ((inc_dir = yasm_get_include_dir(&iter)) != NULL) {
        cpp_arg_entry *arg = yasm_xmalloc(sizeof(cpp_arg_entry));
        arg->op = "-I";
        arg->param = yasm__xstrdup(inc_dir);

        TAILQ_INSERT_TAIL(&pp->cpp_args, arg, entry);
    }

    return (yasm_preproc *)pp;
}

static void
cpp_preproc_destroy(yasm_preproc *preproc)
{
    yasm_preproc_cpp *pp = (yasm_preproc_cpp *)preproc;

    int failed = 0;

    if (pp->f) {
#ifdef HAVE_POPEN
        failed = pclose(pp->f) != 0;
#endif
    }
    if (pp->ms_input) {
        remove(pp->ms_input);
        yasm_xfree(pp->ms_input);
        pp->ms_input = NULL;
    }
    if (failed)
        yasm__fatal( N_("Preprocessor exited with failure") );

    cpp_destroy_args(pp);

    while (pp->ms_once) {
        cpp_once_entry *once = pp->ms_once;
        pp->ms_once = once->next;
        yasm_xfree(once->path);
        yasm_xfree(once);
    }
    yasm_xfree(pp->filename);
    yasm_xfree(pp);
}

static char *
cpp_preproc_get_line(yasm_preproc *preproc)
{
    yasm_preproc_cpp *pp = (yasm_preproc_cpp *)preproc;
    int bufsize = BSIZE;
    char *buf, *p;

    if (! (pp->flags & CPP_HAS_BEEN_INVOKED) ) {
        pp->flags |= CPP_HAS_BEEN_INVOKED;

        cpp_invoke(pp);
    }

    /*
        Once the preprocessor has been run, we're just dealing with a normal
        file.
    */

    /* Loop to ensure entire line is read (don't want to limit line length). */
    buf = yasm_xmalloc((size_t)bufsize);
    p = buf;
    for (;;) {
        if (!fgets(p, bufsize-(p-buf), pp->f)) {
            if (ferror(pp->f)) {
                yasm_error_set(YASM_ERROR_IO,
                               N_("error when reading from file"));
                yasm_errwarn_propagate(pp->errwarns,
                    yasm_linemap_get_current(pp->cur_lm));
            }
            break;
        }
        p += strlen(p);
        if (p > buf && p[-1] == '\n')
            break;
        if ((p-buf) >= bufsize) {
            /* Increase size of buffer */
            char *oldbuf = buf;
            bufsize *= 2;
            buf = yasm_xrealloc(buf, (size_t)bufsize);
            p = buf + (p-oldbuf);
        }
    }

    if (p == buf) {
        /* No data; must be at EOF */
        yasm_xfree(buf);
        return NULL;
    }

    /* Strip the line ending */
    buf[strcspn(buf, "\r\n")] = '\0';

    /* replace ms-style #line \d+ directives with gcc-style */
    if (pp->flags & CPP_HAS_MS_CPP) {
        p = buf;
        while (*p == ' ' || *p == '\t')
            ++p;
        if (0 == strncmp(p, "#line ", 6) && (p[6] >= '0' && p[6] <= '9')) /* erase "line" */
        {
            p += 1;
            size_t len = strlen(p);
            memmove(p, p + 4, len - 4 + 1); /* + 1 for trailing \0 */
        }
    }
    return buf;
}

static size_t
cpp_preproc_get_included_file(yasm_preproc *preproc, char *buf,
                              size_t max_size)
{
    char *p = buf;
    int ch = '\0';
    size_t n = 0;
    yasm_preproc_cpp *pp = (yasm_preproc_cpp *)preproc;

    if (! (pp->flags & CPP_HAS_GENERATED_DEPS) ) {
        pp->flags |= CPP_HAS_GENERATED_DEPS;

        cpp_generate_deps(pp);

        /* Skip target name and first dependency. */
        while (ch != ':')
            ch = fgetc(pp->f_deps);

        fgetc(pp->f_deps);      /* Discard space after colon. */

        while (ch != ' ' && ch != EOF)
            ch = fgetc(pp->f_deps);

        if (ch == EOF)
            return 0;
    }

    while (n < max_size) {
        ch = fgetc(pp->f_deps);

        if (ch == ' ' || ch == EOF) {
            *p = '\0';
            return n;
        }

        /* Eat any silly characters. */
        if (ch < ' ')
            continue;

        *p++ = ch;
        n++;
    }

    /* Ensure the buffer is null-terminated. */
    *(p - 1) = '\0';
    return n;
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

static void
cpp_preproc_add_standard(yasm_preproc *preproc, const char **macros)
{
    /* TODO */
}

/*******************************************************************************
    Preprocessor module object.
*******************************************************************************/

yasm_preproc_module yasm_cpp_LTX_preproc = {
    "Run input through external C preprocessor",
    "cpp",
    cpp_preproc_create,
    cpp_preproc_destroy,
    cpp_preproc_get_line,
    cpp_preproc_get_included_file,
    cpp_preproc_add_include_file,
    cpp_preproc_predefine_macro,
    cpp_preproc_undefine_macro,
    cpp_preproc_define_builtin,
    cpp_preproc_add_standard
};
