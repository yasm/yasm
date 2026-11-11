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

typedef struct cpp_path_entry {
    struct cpp_path_entry *next;
    char *path;
} cpp_path_entry;

typedef struct yasm_preproc_cpp {
    yasm_preproc_base preproc;   /* base structure */

    /* List of arguments to pass to cpp. */
    TAILQ_HEAD(cpp_arg_head, cpp_arg_entry) cpp_args;

    /* The preprocessor's part of every command line: the program, and for --cpp in gcc mode its -E. */
    char *command;

    char *filename;
    FILE *f, *f_deps;
    yasm_linemap *cur_lm;
    yasm_errwarns *errwarns;

    int flags;

    /* The temporary file cl reads instead of filename, and the "#pragma once" files inlined into it. */
    char *ms_input;
    cpp_path_entry *ms_once;

    /* The files cpp_ms_flatten inlined, for the dependency list, and the next one to report. */
    cpp_path_entry *ms_deps, *ms_dep_next;

    /* The whole -M output of a gcc-style preprocessor, and the next file in it to report. */
    char *deps_text;
    const char *deps_next;

    /* The object format (-f), from the __YASM_OBJFMT__ builtin; NULL until the frontend defines it. */
    char *objfmt;
} yasm_preproc_cpp;

/* Flag values for yasm_preproc_cpp->flags. */
#define CPP_HAS_BEEN_INVOKED        0x01
#define CPP_HAS_GENERATED_DEPS      0x02
#define CPP_HAS_MS_CPP              0x04 /* use MS compiler for cpp */
#define CPP_GETS_TARGET             0x08 /* pass -m32 or -m64 as the object format says */

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

/* Append a string as one quoted argument. */
#define APPEND_QUOTED(s) do {                       \
    char *_quoted = cpp_quote(s);                   \
    APPEND(_quoted);                                \
    yasm_xfree(_quoted);                            \
} while (0)

/*
    Whether the program, a name or a path len characters long, takes cl's options; only on Windows. 1 for cl and icl,
    which come as one program per target, 2 for clang-cl and icx, which serve every target; 0 otherwise.
*/
static int
cpp_is_ms_program(const char *prog, size_t len)
{
#ifdef _WIN32
    static const char *ms_names[] = { "cl", "icl", "clang-cl", "icx-cl", "icx" };
    const char *name = prog, *p;
    size_t i, n;

    for (p = prog; p < prog + len; p++) {
        if (*p == '/' || *p == '\\' || *p == ':')
            name = p + 1;
    }
    n = (size_t)(prog + len - name);
    if (n > 4 && !yasm__strncasecmp(name + n - 4, ".exe", 4))
        n -= 4;
    for (i = 0; i < NELEMS(ms_names); i++) {
        if (strlen(ms_names[i]) == n && !yasm__strncasecmp(name, ms_names[i], n))
            return i < 2 ? 1 : 2;
    }
#else
    (void)prog;
    (void)len;
#endif
    return 0;
}

/* The program of a command such as CPP_PROG: its first word, or the text of a quoted first word. */
static const char *
cpp_command_program(const char *cmd, size_t *len)
{
    const char *end;

    if (*cmd == '"' || *cmd == '\'') {
        end = strchr(cmd + 1, *cmd);
        if (end) {
            *len = (size_t)(end - cmd - 1);
            return cmd + 1;
        }
    }
    *len = strcspn(cmd, " \t");
    return cmd;
}

/* The text as one argument for the shell popen runs: in double quotes for cmd, in single quotes for sh. */
static char *
cpp_quote(const char *text)
{
    char *quoted = yasm_xmalloc(strlen(text) * 4 + 3), *q = quoted;
#ifdef _WIN32
    size_t backslashes = 0;

    *q++ = '"';
    for (; *text; text++) {
        backslashes = *text == '\\' ? backslashes + 1 : 0;
        *q++ = *text;
    }
    /* Backslashes right before the closing quote would escape it. */
    while (backslashes--)
        *q++ = '\\';
    *q++ = '"';
#else
    *q++ = '\'';
    for (; *text; text++) {
        if (*text == '\'') {
            memcpy(q, "'\\''", 4);
            q += 4;
        } else
            *q++ = *text;
    }
    *q++ = '\'';
#endif
    *q = '\0';
    return quoted;
}

/*
    The command part for a program named by path or name, quoted; its name decides the mode. Except cl and icl, whose
    target is the program itself, it is told the target of the object format (CPP_GETS_TARGET).
*/
static char *
cpp_program_command(yasm_preproc_cpp *pp, const char *prog)
{
    char *quoted, *command;
    int ms;

#ifdef _WIN32
    /* cmd expands %NAME% even inside quotes, and a quote cannot be escaped. */
    if (strpbrk(prog, "\"%"))
        yasm__fatal(N_("C preprocessor `%s' contains \" or %%"), prog);
#endif
    quoted = cpp_quote(prog);
    ms = cpp_is_ms_program(prog, strlen(prog));
    if (ms != 1)
        pp->flags |= CPP_GETS_TARGET;
    if (ms) {
        pp->flags |= CPP_HAS_MS_CPP;
        return quoted;
    }
    command = yasm_xmalloc(strlen(quoted) + 4);
    sprintf(command, "%s -E", quoted);
    yasm_xfree(quoted);
    return command;
}

#ifdef _WIN32
static int
cpp_file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (!f)
        return 0;
    fclose(f);
    return 1;
}

/* dir, if not empty, then name, then ext, when that file exists; NULL otherwise. */
static char *
cpp_existing_file(const char *dir, size_t dir_len, const char *name, size_t name_len, const char *ext)
{
    char *path = yasm_xmalloc(dir_len + name_len + strlen(ext) + 2), *p = path;

    if (dir_len > 0) {
        memcpy(p, dir, dir_len);
        p += dir_len;
        if (dir[dir_len-1] != '\\' && dir[dir_len-1] != '/')
            *p++ = '\\';
    }
    memcpy(p, name, name_len);
    strcpy(p + name_len, ext);
    if (cpp_file_exists(path))
        return path;
    yasm_xfree(path);
    return NULL;
}

/*
    The next folder of a PATH list, without its quotes, in *dir and *len; 0 when none is left. Only absolute folders
    count, never the current directory, and none with " or % in it, which cmd cannot be given in quotes.
*/
static int
cpp_next_path_dir(const char **list, const char **dir, size_t *len)
{
    while (**list) {
        const char *start = *list, *end = strchr(start, ';');

        if (!end)
            end = start + strlen(start);
        *list = *end ? end + 1 : end;
        if (*start == '"' && end - start >= 2 && end[-1] == '"') {
            start++;
            end--;
        }
        if (end - start < 3 || memchr(start, '"', (size_t)(end - start)) || memchr(start, '%', (size_t)(end - start)))
            continue;
        if ((start[1] == ':' && (start[2] == '\\' || start[2] == '/')) ||
            ((start[0] == '\\' || start[0] == '/') && (start[1] == '\\' || start[1] == '/'))) {
            *dir = start;
            *len = (size_t)(end - start);
            return 1;
        }
    }
    return 0;
}

/* The program as a path, or a name found in a PATH folder as cmd would run it; NULL if it is not there. */
static char *
cpp_find_program(const char *prog, size_t len)
{
    static const char *exts[] = { ".exe", ".com", ".bat", ".cmd" };
    const char *name = prog, *list = getenv("PATH"), *dir = NULL, *p;
    size_t dir_len = 0, i;
    int has_ext;
    char *path;

    for (p = prog; p < prog + len; p++) {
        if (*p == '/' || *p == '\\' || *p == ':')
            name = p + 1;
    }
    has_ext = memchr(name, '.', (size_t)(prog + len - name)) != NULL;
    do {
        if (name == prog && (!list || !cpp_next_path_dir(&list, &dir, &dir_len)))
            return NULL;
        for (i = 0; i < (has_ext ? 1 : NELEMS(exts)); i++) {
            path = cpp_existing_file(dir, dir_len, prog, len, has_ext ? "" : exts[i]);
            if (path)
                return path;
        }
    } while (name == prog);
    return NULL;
}

/* The first cl, clang-cl, icx-cl, icx, icl, clang, gcc or cpp in the PATH folders, in PATH order; NULL if none. */
static char *
cpp_search_path(void)
{
    static const char *names[] = { "cl", "clang-cl", "icx-cl", "icx", "icl", "clang", "gcc", "cpp" };
    const char *list = getenv("PATH"), *dir;
    size_t len, i;
    char *path;

    if (!list)
        return NULL;
    while (cpp_next_path_dir(&list, &dir, &len)) {
        for (i = 0; i < NELEMS(names); i++) {
            path = cpp_existing_file(dir, len, names[i], strlen(names[i]), ".exe");
            if (path)
                return path;
        }
    }
    return NULL;
}
#endif

/* The preprocessor --cpp names, else CPP_PROG as it is; on Windows, if CPP_PROG is not there, one found on PATH. */
static char *
cpp_choose_command(yasm_preproc_cpp *pp)
{
    const char *prog = yasm_get_cpp_program(), *cpp_prog = CPP_PROG;
    size_t len;
#ifdef _WIN32
    char *found, *command;
#endif

    if (prog) {
        if (!*prog)
            yasm__fatal(N_("--cpp needs a program"));
#ifdef _WIN32
        /* A bare name is looked up here: cmd would run one in the current directory first. */
        if (!strpbrk(prog, "/\\:")) {
            found = cpp_find_program(prog, strlen(prog));
            if (!found)
                yasm__fatal(N_("C preprocessor `%s' is not in any PATH folder"), prog);
            command = cpp_program_command(pp, found);
            yasm_xfree(found);
            return command;
        }
#endif
        return cpp_program_command(pp, prog);
    }
    prog = cpp_command_program(cpp_prog, &len);
#ifdef _WIN32
    found = cpp_find_program(prog, len);
    if (!found) {
        found = cpp_search_path();
        if (!found)
            yasm__fatal(N_("no C preprocessor: `%s' is not there and PATH has no cl, clang-cl, icx-cl, icx, icl, "
                           "clang, gcc or cpp; name one with --cpp"), CPP_PROG);
        command = cpp_program_command(pp, found);
        yasm_xfree(found);
        return command;
    }
#endif
    if (cpp_is_ms_program(prog, len))
        pp->flags |= CPP_HAS_MS_CPP;
#ifdef _WIN32
    {
        /* CPP_PROG's program by the path found, then its arguments: cmd would look in the current directory first. */
        const char *args = prog + len + (prog != cpp_prog ? 1 : 0);
        char *quoted = cpp_quote(found);

        command = yasm_xmalloc(strlen(quoted) + strlen(args) + 1);
        sprintf(command, "%s%s", quoted, args);
        yasm_xfree(quoted);
        yasm_xfree(found);
        return command;
    }
#else
    return yasm__xstrdup(CPP_PROG);
#endif
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
cpp_path_listed(const cpp_path_entry *list, const char *path)
{
    for (; list; list = list->next) {
        if (strcmp(list->path, path) == 0)
            return 1;
    }
    return 0;
}

static void
cpp_list_path(cpp_path_entry **list, const char *path)
{
    cpp_path_entry *entry = yasm_xmalloc(sizeof(cpp_path_entry));

    entry->next = NULL;
    entry->path = yasm__xstrdup(path);
    while (*list)
        list = &(*list)->next;
    *list = entry;
}

static void
cpp_free_paths(cpp_path_entry **list)
{
    while (*list) {
        cpp_path_entry *entry = *list;

        *list = entry->next;
        yasm_xfree(entry->path);
        yasm_xfree(entry);
    }
}

/*
    Writes the file to out with every header it includes inlined in its place, so that cpp_ms_put_line reaches the
    macros defined in headers too; #line markers keep cl's file names and line numbers those of the real files. An
    include that cannot be found here stays a directive for cl to resolve. Every header inlined goes to ms_deps; with
    out NULL that is all it does.
*/
static void
cpp_ms_flatten(yasm_preproc_cpp *pp, FILE *in, const char *path, FILE *out, int depth)
{
    size_t size = BSIZE;
    char *buf = yasm_xmalloc(size);
    unsigned long line = 0;

    if (out)
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
            if (!cpp_path_listed(pp->ms_deps, inc_path))
                cpp_list_path(&pp->ms_deps, inc_path);
            if (!cpp_path_listed(pp->ms_once, inc_path))
                cpp_ms_flatten(pp, inc, inc_path, out, depth + 1);
            fclose(inc);
            yasm_xfree(inc_path);
            if (out)
                cpp_ms_line_marker(out, line + 1, path);
        } else {
            if (cpp_is_pragma_once(buf) && !cpp_path_listed(pp->ms_once, path))
                cpp_list_path(&pp->ms_once, path);
            if (out)
                cpp_ms_put_line(out, buf);
        }
        if (name)
            yasm_xfree(name);
    }
    yasm_xfree(buf);
}

/* Flattens the -P files, as cl has no -include, then the input ("-" is standard input) into out, or only lists. */
static void
cpp_ms_flatten_input(yasm_preproc_cpp *pp, FILE *out)
{
    cpp_arg_entry *arg;
    char *path;
    FILE *in;

    for (arg = TAILQ_FIRST(&pp->cpp_args); arg; arg = TAILQ_NEXT(arg, entry)) {
        if (strcmp(arg->op, "-include") != 0)
            continue;
        in = cpp_ms_open_include(pp, "", arg->param, 0, &path);
        if (!in)
            yasm__fatal(N_("could not open file `%s'"), arg->param);
        if (!cpp_path_listed(pp->ms_deps, path))
            cpp_list_path(&pp->ms_deps, path);
        cpp_ms_flatten(pp, in, path, out, 0);
        fclose(in);
        yasm_xfree(path);
    }
    if (strcmp(pp->filename, "-") == 0) {
        cpp_ms_flatten(pp, stdin, pp->filename, out, 0);
        return;
    }
    in = fopen(pp->filename, "r");
    if (!in)
        yasm__fatal(N_("could not open file `%s'"), pp->filename);
    cpp_ms_flatten(pp, in, pp->filename, out, 0);
    fclose(in);
}

/*
    Writes the input, flattened, to a new file in the temporary directory and returns its path. The name holds the
    process id and the file is created only if it does not exist yet, so parallel builds never share one.
*/
static char *
cpp_ms_prepare_input(yasm_preproc_cpp *pp)
{
    const char *dir = getenv("TMP");
    char *path;
    FILE *out = NULL;
#ifdef _WIN32
    unsigned int n;
#endif

    if (!dir || !*dir)
        dir = getenv("TEMP");
    if (!dir || !*dir)
        dir = ".";
    path = yasm_xmalloc(strlen(dir) + 48);
#ifdef _WIN32
    /* .S: clang-cl and icx ignore an .asm or .s input, and preprocess .S as assembler. */
    for (n = 0; n < 1000 && !out; n++) {
        sprintf(path, "%s\\yasm-%d-%u.S", dir, _getpid(), n);
        out = fopen(path, "wx");
    }
#endif
    if (!out)
        yasm__fatal(N_("could not create a temporary file for the preprocessor"));

    cpp_ms_flatten_input(pp, out);
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
    size_t cpp_len = strlen(pp->command);

    /* Initialize command line. */
    cmdline = p = yasm_xmalloc(cpp_len +CMDLINE_SIZE);
    limit = p + CMDLINE_SIZE;
#ifdef _WIN32
    /* popen runs cmd /c, which drops the first and the last quote of a line that starts with one */
    *p++ = '"';
#endif
    strcpy(p, pp->command);
    p += cpp_len;

    if ((pp->flags & CPP_GETS_TARGET) && pp->objfmt) {
        if (!strcmp(pp->objfmt, "win32") || !strcmp(pp->objfmt, "coff") || !strcmp(pp->objfmt, "elf32") ||
            !strcmp(pp->objfmt, "macho32"))
            APPEND(" -m32");
        else if (!strcmp(pp->objfmt, "win64") || !strcmp(pp->objfmt, "x64") || !strcmp(pp->objfmt, "elf64") ||
                 !strcmp(pp->objfmt, "macho64"))
            APPEND(" -m64");
        else if (!strcmp(pp->objfmt, "elfx32"))
            APPEND(" -mx32");
    }

    arg = TAILQ_FIRST(&pp->cpp_args);

    /* Append arguments from the list; the paths quoted, the macros as they were given. -P files are already in the
       input of an MS preprocessor. */
    while ( arg ) {
        if (!(pp->flags & CPP_HAS_MS_CPP) || strcmp(arg->op, "-include") != 0) {
            APPEND(" ");
            APPEND(arg->op);
            APPEND(" ");
            if (strcmp(arg->op, "-D") == 0 || strcmp(arg->op, "-U") == 0)
                APPEND(arg->param);
            else
                APPEND_QUOTED(arg->param);
        }

        arg = TAILQ_NEXT(arg, entry);
    }

    /* Append extra arguments. */
    if (extra) {
        APPEND(" ");
        APPEND(extra);
    }
    /* Append final arguments. */
    if (pp->flags & CPP_HAS_MS_CPP)
        APPEND(" /nologo /E ");
    else
        APPEND(" -x assembler-with-cpp ");
    APPEND_QUOTED(input);
#ifdef _WIN32
    APPEND("\"");
#endif

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

static int
cpp_is_blank(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/*
    The next file of the -M output into buf, or with buf NULL only skipped; returns its length, 0 at the end. Files
    are separated by blanks and backslash line ends; "\ " is a space in a name, "$$" a $.
*/
static size_t
cpp_next_dep(yasm_preproc_cpp *pp, char *buf, size_t max_size)
{
    const char *p = pp->deps_next;
    size_t n = 0;

    for (;;) {
        if (cpp_is_blank(*p))
            p++;
        else if (p[0] == '\\' && (p[1] == '\n' || (p[1] == '\r' && p[2] == '\n')))
            p += p[1] == '\n' ? 2 : 3;
        else
            break;
    }
    while (*p && !cpp_is_blank(*p)) {
        char c = *p++;

        if (c == '\\' && (*p == ' ' || *p == '#'))
            c = *p++;
        else if (c == '$' && *p == '$')
            p++;
        if (buf && n + 1 < max_size)
            buf[n] = c;
        n++;
    }
    pp->deps_next = p;
    if (buf && max_size > 0)
        buf[n < max_size ? n : max_size - 1] = '\0';
    return n;
}

/* Reads the whole -M output, then skips its target and the source file, which come first. */
static void
cpp_read_deps(yasm_preproc_cpp *pp)
{
    size_t size = BSIZE, len = 0, got;
    char *text = yasm_xmalloc(size), *p;
    int failed = 0;

    cpp_generate_deps(pp);
    while ((got = fread(text + len, 1, size - len - 1, pp->f_deps)) > 0) {
        len += got;
        if (len + 1 == size) {
            size *= 2;
            text = yasm_xrealloc(text, size);
        }
    }
    text[len] = '\0';
#ifdef HAVE_POPEN
    failed = pclose(pp->f_deps) != 0;
#endif
    pp->f_deps = NULL;
    pp->deps_text = text;
    if (failed)
        yasm__fatal( N_("Preprocessor exited with failure") );

    /* The target ends at the first colon followed by a blank; the one in C:\ is not. */
    for (p = text; *p && !(*p == ':' && (p[1] == '\0' || cpp_is_blank(p[1]))); p++)
        ;
    pp->deps_next = *p ? p + 1 : p;
    cpp_next_dep(pp, NULL, 0);
}

/* cl has no -M: the included files are the ones cpp_ms_flatten inlines, listed when it made cl's input. */
static size_t
cpp_ms_get_included_file(yasm_preproc_cpp *pp, char *buf, size_t max_size)
{
    cpp_path_entry *dep;
    size_t n;

    if (!(pp->flags & CPP_HAS_GENERATED_DEPS)) {
        pp->flags |= CPP_HAS_GENERATED_DEPS;
        if (!(pp->flags & CPP_HAS_BEEN_INVOKED))
            cpp_ms_flatten_input(pp, NULL);
        pp->ms_dep_next = pp->ms_deps;
    }

    dep = pp->ms_dep_next;
    if (!dep || max_size == 0)
        return 0;
    pp->ms_dep_next = dep->next;
    n = strlen(dep->path);
    if (n >= max_size)
        n = max_size - 1;
    memcpy(buf, dep->path, n);
    buf[n] = '\0';
    return n;
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
    pp->flags = 0;
    pp->command = cpp_choose_command(pp);
    pp->filename = yasm__xstrdup(in);
    pp->ms_input = NULL;
    pp->ms_once = NULL;
    pp->ms_deps = pp->ms_dep_next = NULL;
    pp->deps_text = NULL;
    pp->deps_next = NULL;
    pp->objfmt = NULL;

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

    cpp_free_paths(&pp->ms_once);
    cpp_free_paths(&pp->ms_deps);
    if (pp->deps_text)
        yasm_xfree(pp->deps_text);
    if (pp->objfmt)
        yasm_xfree(pp->objfmt);
    yasm_xfree(pp->command);
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
            memmove(p + 1, p + 5, strlen(p + 5) + 1); /* + 1 for trailing \0 */
    }
    return buf;
}

static size_t
cpp_preproc_get_included_file(yasm_preproc *preproc, char *buf,
                              size_t max_size)
{
    yasm_preproc_cpp *pp = (yasm_preproc_cpp *)preproc;

    if (pp->flags & CPP_HAS_MS_CPP)
        return cpp_ms_get_included_file(pp, buf, max_size);

    if (! (pp->flags & CPP_HAS_GENERATED_DEPS) ) {
        pp->flags |= CPP_HAS_GENERATED_DEPS;
        cpp_read_deps(pp);
    }
    return cpp_next_dep(pp, buf, max_size);
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
    yasm_preproc_cpp *pp = (yasm_preproc_cpp *)preproc;

    if (strncmp(macronameval, "__YASM_OBJFMT__=", 16) == 0) {
        if (pp->objfmt)
            yasm_xfree(pp->objfmt);
        pp->objfmt = yasm__xstrdup(macronameval + 16);
    }
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
