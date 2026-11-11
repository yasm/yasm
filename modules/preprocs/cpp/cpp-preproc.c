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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

/* The longest command line CreateProcess takes. */
#define CMDLINE_SIZE 32767

#define BSIZE 512

#ifdef _WIN32
typedef HANDLE cpp_process;
/* Quotes that keep blanks in a word of a command; a ' can be part of a Windows path. */
#define CPP_QUOTES "\""
#else
typedef pid_t cpp_process;
#define CPP_QUOTES "\"'"
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

/* A list of arguments being built, ended by NULL. */
typedef struct cpp_argv {
    char **items;
    size_t count, size;
} cpp_argv;

typedef struct yasm_preproc_cpp {
    yasm_preproc_base preproc;   /* base structure */

    /* List of arguments to pass to cpp. */
    TAILQ_HEAD(cpp_arg_head, cpp_arg_entry) cpp_args;

    /* The preprocessor's part of every run: the program, then the options of its command. */
    cpp_argv command;

    char *filename;
    FILE *f, *f_deps;
    cpp_process process, deps_process;
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

static void
cpp_argv_add(cpp_argv *av, const char *text, size_t len)
{
    if (av->count + 2 > av->size) {
        av->size = av->size ? av->size * 2 : 16;
        av->items = av->items ? yasm_xrealloc(av->items, av->size * sizeof(char *))
                              : yasm_xmalloc(av->size * sizeof(char *));
    }
    av->items[av->count] = yasm_xmalloc(len + 1);
    memcpy(av->items[av->count], text, len);
    av->items[av->count][len] = '\0';
    av->items[++av->count] = NULL;
}

static void
cpp_argv_add_str(cpp_argv *av, const char *text)
{
    cpp_argv_add(av, text, strlen(text));
}

static void
cpp_argv_free(cpp_argv *av)
{
    size_t i;

    for (i = 0; i < av->count; i++)
        yasm_xfree(av->items[i]);
    if (av->items)
        yasm_xfree(av->items);
    av->items = NULL;
    av->count = av->size = 0;
}

/*
    Splits a command such as CPP_PROG or --cpp's into words: blanks separate them, and a quoted part keeps its blanks.
    No shell reads the command, so $, %, ;, & and the like are ordinary characters.
*/
static void
cpp_split_command(const char *cmd, cpp_argv *words)
{
    char *word = yasm_xmalloc(strlen(cmd) + 1), quote;
    size_t len;

    for (;;) {
        while (*cmd == ' ' || *cmd == '\t')
            cmd++;
        if (!*cmd)
            break;
        len = 0;
        quote = 0;
        for (; *cmd && (quote || (*cmd != ' ' && *cmd != '\t')); cmd++) {
            if (quote && *cmd == quote)
                quote = 0;
            else if (!quote && strchr(CPP_QUOTES, *cmd))
                quote = *cmd;
            else
                word[len++] = *cmd;
        }
        cpp_argv_add(words, word, len);
    }
    yasm_xfree(word);
}

/*
    Whether the program, a name or a path len characters long, takes cl's options. 1 for cl and icl, which come as one
    program per target, 2 for clang-cl and icx, which serve every target; 0 otherwise. Outside Windows icx is the
    gcc-style driver, and clang-cl is the only one of them.
*/
static int
cpp_is_ms_program(const char *prog, size_t len)
{
#ifdef _WIN32
    static const char *ms_names[] = { "cl", "icl", "clang-cl", "icx-cl", "icx" };
#else
    static const char *ms_names[] = { "clang-cl" };
#endif
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
            return strcmp(ms_names[i], "cl") == 0 || strcmp(ms_names[i], "icl") == 0 ? 1 : 2;
    }
    return 0;
}

#ifdef _WIN32
/*
    The arguments as one command line that the Microsoft C runtime, and the compilers, split back into the same
    arguments: one with a blank or a quote is put in quotes, with its quotes and the backslashes before them escaped.
*/
static char *
cpp_windows_command_line(char **items)
{
    size_t size = 1, i, backslashes;
    char *line, *p;
    const char *s;

    for (i = 0; items[i]; i++)
        size += strlen(items[i]) * 2 + 3;
    line = p = yasm_xmalloc(size);
    for (i = 0; items[i]; i++) {
        s = items[i];
        if (i > 0)
            *p++ = ' ';
        if (*s && !strpbrk(s, " \t\n\v\"")) {
            strcpy(p, s);
            p += strlen(s);
            continue;
        }
        *p++ = '"';
        backslashes = 0;
        for (; *s; s++) {
            if (*s == '\\') {
                backslashes++;
                continue;
            }
            if (*s == '"')
                backslashes = backslashes * 2 + 1;
            for (; backslashes > 0; backslashes--)
                *p++ = '\\';
            *p++ = *s;
        }
        for (backslashes *= 2; backslashes > 0; backslashes--)
            *p++ = '\\';
        *p++ = '"';
    }
    *p = '\0';
    if ((size_t)(p - line) >= CMDLINE_SIZE)
        yasm__fatal(N_("command line too long!"));
    return line;
}

static int
cpp_is_file(const char *path)
{
    DWORD attrs = GetFileAttributesA(path);

    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

/*
    The program's path, .exe added when it has no extension: a path as given, a name as SearchPath finds it in the
    PATH folders. Searching PATH alone leaves out the current directory, unless PATH names it. NULL if not found.
*/
static char *
cpp_find_program(const char *prog)
{
    const char *list = getenv("PATH"), *name = prog, *p;
    char *found;
    DWORD size;

    for (p = prog; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == ':')
            name = p + 1;
    }
    if (name != prog) {
        found = yasm_xmalloc(strlen(prog) + 5);
        strcpy(found, prog);
        if (!strchr(name, '.'))
            strcat(found, ".exe");
    } else {
        if (!list || !*list)
            return NULL;
        size = SearchPathA(list, prog, ".exe", 0, NULL, NULL);
        if (size == 0)
            return NULL;
        found = yasm_xmalloc(size);
        if (SearchPathA(list, prog, ".exe", size, found, NULL) == 0)
            found[0] = '\0';
    }
    if (cpp_is_file(found))
        return found;
    yasm_xfree(found);
    return NULL;
}

/* The first of the Visual Studio compilers, which all preprocess for Windows, that PATH has; NULL if none. */
static char *
cpp_search_path(void)
{
    static const char *names[] = { "cl", "clang-cl", "icx-cl", "icx", "icl" };
    size_t i;
    char *path;

    for (i = 0; i < NELEMS(names); i++) {
        path = cpp_find_program(names[i]);
        if (path)
            return path;
    }
    return NULL;
}

/*
    Makes the command the compiler the PATH search found. Except cl and icl, whose target is the program itself, it is
    told the target of the object format (CPP_GETS_TARGET).
*/
static void
cpp_use_found(yasm_preproc_cpp *pp, char *path)
{
    pp->flags |= CPP_HAS_MS_CPP;
    if (cpp_is_ms_program(path, strlen(path)) != 1)
        pp->flags |= CPP_GETS_TARGET;
    cpp_argv_add_str(&pp->command, path);
    yasm_xfree(path);
}

/* Whether the program is a batch file, which Windows runs through cmd, and cmd would read its arguments as commands. */
static int
cpp_is_batch_file(const char *path)
{
    size_t len = strlen(path);

    return len > 4 && (!yasm__strcasecmp(path + len - 4, ".bat") || !yasm__strcasecmp(path + len - 4, ".cmd"));
}
#endif

/*
    The command -r cpp runs, as words: --cpp's, else CPP_PROG's. On Windows its program is run by the path found, a
    name looked up in the PATH folders; if CPP_PROG's program is not there, for a win32 or win64 object the first
    Visual Studio compiler on PATH is run instead.
*/
static void
cpp_choose_command(yasm_preproc_cpp *pp)
{
    const char *cmd = yasm_get_cpp_command();
#ifdef _WIN32
    char *found;
#endif

    cpp_split_command(cmd ? cmd : CPP_PROG, &pp->command);
#ifdef _WIN32
    found = pp->command.count ? cpp_find_program(pp->command.items[0]) : NULL;
    if (found) {
        yasm_xfree(pp->command.items[0]);
        pp->command.items[0] = found;
    } else if (!cmd) {
        /* The compilers on PATH preprocess for Windows: their macros would be wrong for an ELF or Mach-O object. */
        if (!pp->objfmt || (strcmp(pp->objfmt, "win32") != 0 && strcmp(pp->objfmt, "win64") != 0 &&
                            strcmp(pp->objfmt, "x64") != 0))
            yasm__fatal(N_("no C preprocessor: `%s' is not there, and PATH is searched only for win32 and win64 "
                           "objects; name one with --cpp"), CPP_PROG);
        cpp_argv_free(&pp->command);
        found = cpp_search_path();
        if (!found)
            yasm__fatal(N_("no C preprocessor: `%s' is not there and PATH has no cl, clang-cl, icx-cl, icx or icl; "
                           "name one with --cpp"), CPP_PROG);
        cpp_use_found(pp, found);
        return;
    } else if (pp->command.count)
        yasm__fatal(N_("C preprocessor `%s' not found"), pp->command.items[0]);
    if (found && cpp_is_batch_file(found))
        yasm__fatal(N_("C preprocessor `%s' is a batch file"), found);
#endif
    if (pp->command.count == 0)
        yasm__fatal(N_("no C preprocessor command in `%s'"), cmd ? cmd : CPP_PROG);
    if (cpp_is_ms_program(pp->command.items[0], strlen(pp->command.items[0])))
        pp->flags |= CPP_HAS_MS_CPP;
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
    char *path;
    FILE *out = NULL;
    unsigned int n;
    int fd;
#ifdef _WIN32
    const char *dir = getenv("TMP");

    if (!dir || !*dir)
        dir = getenv("TEMP");
    if (!dir || !*dir)
        dir = ".";
#else
    const char *dir = getenv("TMPDIR");

    if (!dir || !*dir)
        dir = "/tmp";
#endif
    path = yasm_xmalloc(strlen(dir) + 48);
    /* .S: clang-cl and icx ignore an .asm or .s input, and preprocess .S as assembler. fopen's "wx" is missing from
       msvcrt.dll, which MinGW builds use. */
    for (n = 0; n < 1000 && !out; n++) {
#ifdef _WIN32
        sprintf(path, "%s\\yasm-%d-%u.S", dir, _getpid(), n);
        fd = _open(path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_TEXT, _S_IREAD | _S_IWRITE);
        if (fd >= 0)
            out = _fdopen(fd, "w");
#else
        sprintf(path, "%s/yasm-%d-%u.S", dir, (int)getpid(), n);
        fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd >= 0)
            out = fdopen(fd, "w");
#endif
    }
    if (!out)
        yasm__fatal(N_("could not create a temporary file for the preprocessor"));

    cpp_ms_flatten_input(pp, out);
    if (fclose(out) != 0)
        yasm__fatal(N_("could not write file `%s'"), path);
    return path;
}

/* The arguments of one run: the command, the target, the -I, -D, -U and -include options, extra, then the input. */
static void
cpp_build_argv(yasm_preproc_cpp *pp, const char *extra, const char *input, cpp_argv *av)
{
    cpp_arg_entry *arg;
    size_t i;

    for (i = 0; i < pp->command.count; i++)
        cpp_argv_add_str(av, pp->command.items[i]);

    if (pp->flags & CPP_GETS_TARGET)
        cpp_argv_add_str(av, strcmp(pp->objfmt, "win32") == 0 ? "-m32" : "-m64");

    /* -P files are already in the input of an MS preprocessor. */
    TAILQ_FOREACH(arg, &pp->cpp_args, entry) {
        if (!(pp->flags & CPP_HAS_MS_CPP) || strcmp(arg->op, "-include") != 0) {
            cpp_argv_add_str(av, arg->op);
            cpp_argv_add_str(av, arg->param);
        }
    }

    if (extra)
        cpp_argv_add_str(av, extra);
    if (pp->flags & CPP_HAS_MS_CPP) {
        cpp_argv_add_str(av, "/nologo");
        cpp_argv_add_str(av, "/E");
    } else {
        cpp_argv_add_str(av, "-x");
        cpp_argv_add_str(av, "assembler-with-cpp");
    }
    cpp_argv_add_str(av, input);
}

#ifdef _WIN32
/* An inheritable copy of a standard handle for the preprocessor, or NULL if there is none. */
static HANDLE
cpp_inheritable(HANDLE h)
{
    HANDLE self = GetCurrentProcess(), copy;

    if (h == NULL || h == INVALID_HANDLE_VALUE || !DuplicateHandle(self, h, self, &copy, 0, TRUE, DUPLICATE_SAME_ACCESS))
        return NULL;
    return copy;
}
#endif

/*
    Starts the preprocessor with the arguments, its program first, and returns its standard output to read. No shell
    runs in between: every argument reaches the program exactly as it is.
*/
static FILE *
cpp_start(cpp_argv *av, cpp_process *process)
{
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    HANDLE rd, wr;
    char *line = cpp_windows_command_line(av->items);
    BOOL started;

    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&rd, &wr, &sa, 0))
        yasm__fatal(N_("Failed to execute preprocessor"));
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = cpp_inheritable(GetStdHandle(STD_INPUT_HANDLE));
    si.hStdOutput = wr;
    si.hStdError = cpp_inheritable(GetStdHandle(STD_ERROR_HANDLE));
    started = CreateProcessA(av->items[0], line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    CloseHandle(wr);
    if (si.hStdInput)
        CloseHandle(si.hStdInput);
    if (si.hStdError)
        CloseHandle(si.hStdError);
    yasm_xfree(line);
    if (!started) {
        CloseHandle(rd);
        yasm__fatal(N_("Failed to execute preprocessor"));
    }
    CloseHandle(pi.hThread);
    *process = pi.hProcess;
    return _fdopen(_open_osfhandle((intptr_t)rd, _O_RDONLY), "r");
#else
    int fds[2];
    pid_t pid;

    if (pipe(fds) != 0)
        yasm__fatal(N_("Failed to execute preprocessor"));
    pid = fork();
    if (pid < 0)
        yasm__fatal(N_("Failed to execute preprocessor"));
    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        execvp(av->items[0], av->items);
        fprintf(stderr, "yasm: cannot run `%s': %s\n", av->items[0], strerror(errno));
        _exit(127);
    }
    close(fds[1]);
    *process = pid;
    return fdopen(fds[0], "r");
#endif
}

/* Closes the preprocessor's output and waits for it to end; returns whether it failed. */
static int
cpp_finish(FILE *f, cpp_process process)
{
#ifdef _WIN32
    DWORD code = 1;

    fclose(f);
    WaitForSingleObject(process, INFINITE);
    if (!GetExitCodeProcess(process, &code))
        code = 1;
    CloseHandle(process);
    return code != 0;
#else
    int status;

    fclose(f);
    while (waitpid(process, &status, 0) < 0) {
        if (errno != EINTR)
            return 1;
    }
    return !WIFEXITED(status) || WEXITSTATUS(status) != 0;
#endif
}

/* Invoke the c preprocessor. */
static void
cpp_invoke(yasm_preproc_cpp *pp)
{
    cpp_argv av = { NULL, 0, 0 };

    if (pp->flags & CPP_HAS_MS_CPP) {
        pp->ms_input = cpp_ms_prepare_input(pp);
        cpp_build_argv(pp, NULL, pp->ms_input, &av);
    } else
        cpp_build_argv(pp, NULL, pp->filename, &av);
    pp->f = cpp_start(&av, &pp->process);
    cpp_argv_free(&av);
    if (!pp->f)
        yasm__fatal(N_("Failed to execute preprocessor"));
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
    cpp_argv av = { NULL, 0, 0 };

    cpp_build_argv(pp, "-M", pp->filename, &av);
    pp->f_deps = cpp_start(&av, &pp->deps_process);
    cpp_argv_free(&av);
    if (!pp->f_deps)
        yasm__fatal(N_("Failed to execute preprocessor"));
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
    failed = cpp_finish(pp->f_deps, pp->deps_process);
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
    pp->command.items = NULL;
    pp->command.count = pp->command.size = 0;
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

    if (pp->f)
        failed = cpp_finish(pp->f, pp->process);
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
    cpp_argv_free(&pp->command);
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
        if (pp->command.count == 0)
            cpp_choose_command(pp);

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

    if (pp->command.count == 0)
        cpp_choose_command(pp);
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
