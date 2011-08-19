/*
 * YAPP preprocessor (mimics NASM's preprocessor)
 *
 * Copyright (C) 2001  Michael Urman
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

#include "yapp-preproc.h"
#include "yapp-token.h"

#define ydebug(x) /* printf x */

typedef struct yasm_preproc_yapp {
    yasm_preproc_base preproc;      /* base structure */
} yasm_preproc_yapp;

yasm_preproc_module yasm_yapp_LTX_preproc;

static int saved_length;

static HAMT *macro_table;

static YAPP_Output current_output;
YYSTYPE yapp_preproc_lval;

/*@dependent@*/ yasm_linemap *yapp_preproc_linemap;

/* Build source and macro representations */
static SLIST_HEAD(source_head, source_s) source_head, macro_head, param_head;
static struct source_s {
    SLIST_ENTRY(source_s) next;
    YAPP_Token token;
} *src, *source_tail, *macro_tail, *param_tail;
typedef struct source_s source;

/* don't forget what the nesting level says */
static SLIST_HEAD(output_head, output_s) output_head;
static struct output_s {
    SLIST_ENTRY(output_s) next;
    YAPP_Output out;
} output, *out;

/*****************************************************************************/
/* macro support - to be moved to a separate file later (?)                  */
/*****************************************************************************/
typedef struct YAPP_Macro_s {
    struct source_head macro_head;
    struct source_head param_head;
    enum {
        YAPP_MACRO = 0,
        YAPP_DEFINE
    } type;
    int args;
    int fillargs;
    int expanding;
} YAPP_Macro;

YAPP_Macro *
yapp_macro_insert (char *name, int argc, int fillargs);

void
yapp_macro_error_exists (YAPP_Macro *v);

void
yapp_macro_error_sameargname (YAPP_Macro *v);

YAPP_Macro *
yapp_define_insert (char *name, int argc, int fillargs);

void
yapp_macro_delete (YAPP_Macro *ym);

static YAPP_Macro *
yapp_macro_get (const char *key);

struct source_head *
yapp_define_param_get (HAMT *hamt, const char *key);

static void
replay_saved_tokens(char *ident,
                    struct source_head *from_head,
                    struct source_head *to_head,
                    source **to_tail);

YAPP_Macro *
yapp_macro_insert (char *name, int argc, int fillargs)
{
    YAPP_Macro *ym = yasm_xmalloc(sizeof(YAPP_Macro));
    ym->type = YAPP_MACRO;
    ym->args = argc;
    ym->fillargs = fillargs;

    memcpy(&ym->macro_head, &macro_head, sizeof(macro_head));

    SLIST_INIT(&macro_head);
    macro_tail = SLIST_FIRST(&macro_head);
    return ym;
}

void
yapp_macro_error_exists (YAPP_Macro *v)
{
    if (v) yasm_error_set(YASM_ERROR_VALUE, N_("Redefining macro of the same name %d:%d"), v->type, v->args);
}

void
yapp_macro_error_sameargname (YAPP_Macro *v)
{
    if (v) yasm_error_set(YASM_ERROR_VALUE, N_("Duplicate argument names in macro"));
}

YAPP_Macro *
yapp_define_insert (char *name, int argc, int fillargs)
{
    int zero = 0;
    char *mungename = name;
    YAPP_Macro *ym;

    ym = yapp_macro_get(name);
    if (ym) {
        if ((argc >= 0 && ym->args < 0)
            || (argc < 0 && ym->args >= 0))
        {
            yasm_warn_set(YASM_WARN_PREPROC, N_("Attempted %%define both with and without parameters"));
            return NULL;
        }
    }
    else if (argc >= 0)
    {
        /* insert placeholder for paramlisted defines */
        ym = yasm_xmalloc(sizeof(YAPP_Macro));
        ym->type = YAPP_DEFINE;
        ym->args = argc;
        ym->fillargs = fillargs;
        ym->expanding = 0;
        HAMT_insert(macro_table, name, (void *)ym, &zero, (void (*)(void *))yapp_macro_error_exists);
    }

    /* now for the real one */
    ym = yasm_xmalloc(sizeof(YAPP_Macro));
    ym->type = YAPP_DEFINE;
    ym->args = argc;
    ym->fillargs = fillargs;
    ym->expanding = 0;

    if (argc>=0) {
        mungename = yasm_xmalloc(strlen(name)+8);
        sprintf(mungename, "%s(%d)", name, argc);
    }

    ydebug(("YAPP: +Inserting macro %s\n", mungename));

    memcpy(&ym->macro_head, &macro_head, sizeof(macro_head));
    memcpy(&ym->param_head, &param_head, sizeof(param_head));

    HAMT_insert(macro_table, mungename, (void *)ym, &zero, (void (*)(void *))yapp_macro_error_exists);

    SLIST_INIT(&macro_head);
    SLIST_INIT(&param_head);

    macro_tail = SLIST_FIRST(&macro_head);
    param_tail = SLIST_FIRST(&param_head);

    return ym;
}

void
yapp_macro_delete (YAPP_Macro *ym)
{
    while (!SLIST_EMPTY(&ym->macro_head)) {
        source *s = SLIST_FIRST(&ym->macro_head);
        yasm_xfree(s);
        SLIST_REMOVE_HEAD(&ym->macro_head, next);
    }
    yasm_xfree(ym);
}

static YAPP_Macro *
yapp_macro_get (const char *key)
{
    return (YAPP_Macro *)HAMT_search(macro_table, key);
}

struct source_head *
yapp_define_param_get (HAMT *hamt, const char *key)
{
    return (struct source_head *)HAMT_search(hamt, key);
}

/*****************************************************************************/

void
append_processed_token(source *tok, struct source_head *to_head, source **to_tail);

void
append_token(int token, struct source_head *to_head, source **to_tail);

int
append_to_return(struct source_head *to_head, source **to_tail);

int
eat_through_return(struct source_head *to_head, source **to_tail);

int
yapp_get_ident(const char *synlvl);

void
copy_token(YAPP_Token *tok, struct source_head *to_head, source **to_tail);

void
expand_macro(char *name,
             struct source_head *from_head,
             struct source_head *to_head,
             source **to_tail);

void
expand_token_list(struct source_head *paramexp, struct source_head *to_head, source **to_tail);

static yasm_preproc *
yapp_preproc_create(const char *in_filename, yasm_linemap *lm,
                    yasm_errwarns *errwarns)
{
    FILE *f;
    yasm_preproc_yapp *preproc_yapp = yasm_xmalloc(sizeof(yasm_preproc_yapp));

    if (strcmp(in_filename, "-") != 0) {
        f = fopen(in_filename, "r");
        if (!f)
            yasm__fatal( N_("Could not open input file") );
    }
    else
        f = stdin;

    preproc_yapp->preproc.module = &yasm_yapp_LTX_preproc;

    yapp_preproc_linemap = lm;
    yapp_preproc_current_file = yasm__xstrdup(in_filename);
    yapp_preproc_line_number = 1;
    yapp_lex_initialize(f);
    SLIST_INIT(&output_head);
    SLIST_INIT(&source_head);
    SLIST_INIT(&macro_head);
    SLIST_INIT(&param_head);
    out = yasm_xmalloc(sizeof(output));
    out->out = current_output = YAPP_OUTPUT;
    SLIST_INSERT_HEAD(&output_head, out, next);

    macro_table = HAMT_create(yasm_internal_error_);

    source_tail = SLIST_FIRST(&source_head);
    macro_tail = SLIST_FIRST(&macro_head);
    param_tail = SLIST_FIRST(&param_head);

    append_token(LINE, &source_head, &source_tail);

    return (yasm_preproc *)preproc_yapp;
}

static void
yapp_preproc_destroy(yasm_preproc *preproc)
{
    /* TODO: clean up */
    yasm_xfree(preproc);
}

/* Generate a new level of if* context
 * if val is true, this module of the current level will be output IFF the
 * surrounding one is.
 */
static void
push_if(int val)
{
    out = yasm_xmalloc(sizeof(output));
    out->out = current_output;
    SLIST_INSERT_HEAD(&output_head, out, next);

    switch (current_output)
    {
        case YAPP_OUTPUT:
            current_output = val ? YAPP_OUTPUT : YAPP_NO_OUTPUT;
            break;

        case YAPP_NO_OUTPUT:
        case YAPP_OLD_OUTPUT:
        case YAPP_BLOCKED_OUTPUT:
            current_output = YAPP_BLOCKED_OUTPUT;
            break;
    }
    if (current_output != YAPP_OUTPUT) set_inhibit();

}

/* Generate a new module in the current if* context
 * if val is true and this level hasn't had a true, it will be output if the
 * surrounding level is.
 */
static void
push_else(int val)
{
    switch (current_output)
    {
        /* if it was NO, turn to output IFF val is true */
        case YAPP_NO_OUTPUT:
            current_output = val ? YAPP_OUTPUT : YAPP_NO_OUTPUT;
            break;

        /* if it was yes, make it OLD */
        case YAPP_OUTPUT:
            current_output = YAPP_OLD_OUTPUT;
            break;

        /* leave OLD as OLD, BLOCKED as BLOCKED */
        case YAPP_OLD_OUTPUT:
        case YAPP_BLOCKED_OUTPUT:
            break;
    }
    if (current_output != YAPP_OUTPUT) set_inhibit();
}

/* Clear the curent if* context level */
static void
pop_if(void)
{
    out = SLIST_FIRST(&output_head);
    current_output = out->out;
    SLIST_REMOVE_HEAD(&output_head, next);
    yasm_xfree(out);
    if (current_output != YAPP_OUTPUT) set_inhibit();
}

/* undefine a symbol */
static void
yapp_undef(const char *key)
{
    int zero = 0;
    HAMT_insert(macro_table, key, NULL, &zero, (void (*)(void *))yapp_macro_delete);
}

/* Is a symbol known to YAPP? */
static int
yapp_defined(const char *key)
{
    return yapp_macro_get(key) != NULL;
}

void
append_processed_token(source *tok, struct source_head *to_head, source **to_tail)
{
    ydebug(("YAPP: appended token \"%s\"\n", tok->token.str));
    if (*to_tail) {
        SLIST_INSERT_AFTER(*to_tail, tok, next);
    }
    else {
        SLIST_INSERT_HEAD(to_head, tok, next);
    }
    *to_tail = tok;
    if (to_head == &source_head)
        saved_length += strlen(tok->token.str);
}

void
append_token(int token, struct source_head *to_head, source **to_tail)
{
    if (current_output != YAPP_OUTPUT) {
        ydebug(("YAPP: append_token while not YAPP_OUTPUT\n"));
        return;
    }

    /* attempt to condense LINES together or newlines onto LINES */
    if ((*to_tail) && (*to_tail)->token.type == LINE
        && (token == '\n' || token == LINE))
    {
        yasm_xfree ((*to_tail)->token.str);
        (*to_tail)->token.str = yasm_xmalloc(23+strlen(yapp_preproc_current_file));
        sprintf((*to_tail)->token.str, "%%line %d+1 %s\n", yapp_preproc_line_number, yapp_preproc_current_file);
    }
    else {
        src = yasm_xmalloc(sizeof(source));
        src->token.type = token;
        switch (token)
        {
            case INTNUM:
                src->token.str = yasm__xstrdup(yapp_preproc_lval.int_str_val.str);
                src->token.val.int_val = yapp_preproc_lval.int_str_val.val;
                break;

            case FLTNUM:
                src->token.str = yasm__xstrdup(yapp_preproc_lval.double_str_val.str);
                src->token.val.double_val = yapp_preproc_lval.double_str_val.val;
                break;

            case STRING:
            case WHITESPACE:
                src->token.str = yasm__xstrdup(yapp_preproc_lval.str_val);
                break;

            case IDENT:
                src->token.str = yasm__xstrdup(yapp_preproc_lval.str_val);
                break;

            case '+': case '-': case '*': case '/': case '%': case ',': case '\n':
            case '[': case ']': case '(': case ')':
                src->token.str = yasm_xmalloc(2);
                src->token.str[0] = (char)token;
                src->token.str[1] = '\0';
                break;

            case LINE:
                /* TODO: consider removing any trailing newline or LINE tokens */
                src->token.str = yasm_xmalloc(23+strlen(yapp_preproc_current_file));
                sprintf(src->token.str, "%%line %d+1 %s\n", yapp_preproc_line_number, yapp_preproc_current_file);
                break;

            default:
                yasm_xfree(src);
                return;
        }
        append_processed_token(src, to_head, to_tail);
    }
}

static void
replay_saved_tokens(char *ident,
                    struct source_head *from_head,
                    struct source_head *to_head,
                    source **to_tail)
{
    source *item, *after;
    ydebug(("-No, %s didn't match any macro we have\n", ident));
    /* this means a macro expansion failed.  stick its name and all the saved
     * tokens into the output stream */
    yapp_preproc_lval.str_val = ident;
    append_token(IDENT, to_head, to_tail);

    item = SLIST_FIRST(from_head);
    while (item) {
        after = SLIST_NEXT(item, next);
        SLIST_INSERT_AFTER(*to_tail, item, next);
        *to_tail = item;
        saved_length += strlen(item->token.str);
        item = after;
    }
    SLIST_INIT(from_head);
}

int
append_to_return(struct source_head *to_head, source **to_tail)
{
    int token = yapp_preproc_lex();
    while (token != '\n') {
        ydebug(("YAPP: ATR: '%c' \"%s\"\n", token, yapp_preproc_lval.str_val));
        if (token == 0)
            return 0;
        append_token(token, to_head, to_tail);
        token = yapp_preproc_lex();
    }
    return '\n';
}

int
eat_through_return(struct source_head *to_head, source **to_tail)
{
    int token;
    while ((token = yapp_preproc_lex()) != '\n') {
        if (token == 0)
            return 0;
        yasm_error_set(YASM_ERROR_SYNTAX,
                       N_("Skipping possibly valid %%define stuff"));
    }
    append_token('\n', to_head, to_tail);
    return '\n';
}

int
yapp_get_ident(const char *synlvl)
{
    int token = yapp_preproc_lex();
    if (token == WHITESPACE)
        token = yapp_preproc_lex();
    if (token != IDENT) {
        yasm_error_set(YASM_ERROR_SYNTAX, N_("Identifier expected after %%%s"),
                       synlvl);
    }
    return token;
}

void
copy_token(YAPP_Token *tok, struct source_head *to_head, source **to_tail)
{
    src = yasm_xmalloc(sizeof(source));
    src->token.type = tok->type;
    src->token.str = yasm__xstrdup(tok->str);

    append_processed_token(src, to_head, to_tail);
}

void
expand_macro(char *name,
             struct source_head *from_head,
             struct source_head *to_head,
             source **to_tail)
{
    struct source_head replay_head, arg_head;
    source *replay_tail, *arg_tail;

    YAPP_Macro *ym = yapp_macro_get(name);

    ydebug(("YAPP: +Expand macro %s...\n", name));

    if (ym->expanding) yasm_internal_error(N_("Recursively expanding a macro!"));

    if (ym->type == YAPP_DEFINE) {
        if (ym->args == -1) {
            /* no parens to deal with */
            ym->expanding = 1;
            expand_token_list(&ym->macro_head, to_head, to_tail);
            ym->expanding = 0;
        }
        else
        {
            char *mungename;
            int token;
            int argc=0;
            int parennest=0;
            HAMT *param_table;
            source *replay, *param;

            ydebug(("YAPP: +Expanding multi-arg macro %s...\n", name));

            /* Build up both a parameter reference list and a token buffer,
             * because we won't know until we've reached the closing paren if
             * we can actuall expand the macro or not.  bleah. */
            /* worse, we can't build the parameter list until we know the
             * parameter names, which we can't look up until we know the
             * number of arguments.  sigh */
            /* HMM */
            SLIST_INIT(&replay_head);
            replay_tail = SLIST_FIRST(&replay_head);

            /* find out what we got */
            if (from_head) {
                yasm_internal_error(N_("Expanding macro with non-null from_head ugh\n"));
            }
            token = yapp_preproc_lex();
            append_token(token, &replay_head, &replay_tail);
            /* allow one whitespace */
            if (token == WHITESPACE) {
                ydebug(("Ignoring WS between macro and paren\n"));
                token = yapp_preproc_lex();
                append_token(token, &replay_head, &replay_tail);
            }
            if (token != '(') {
                ydebug(("YAPP: -Didn't get to left paren; instead got '%c' \"%s\"\n", token, yapp_preproc_lval.str_val));
                replay_saved_tokens(name, &replay_head, to_head, to_tail);
                return;
            }

            /* at this point, we've got the left paren.  time to get annoyed */
            while (token != ')') {
                token = yapp_preproc_lex();
                append_token(token, &replay_head, &replay_tail);
                /* TODO: handle { } for commas?  or is that just macros? */
                switch (token) {
                    case '(':
                        parennest++;
                        break;

                    case ')':
                        if (parennest) {
                            parennest--;
                            token = ',';
                        }
                        else {
                            argc++;
                        }
                        break;

                    case ',':
                        if (!parennest)
                            argc++;
                        break;

                    case WHITESPACE: case IDENT: case INTNUM: case FLTNUM: case STRING:
                    case '+': case '-': case '*': case '/':
                        break;

                    default:
                        if (token < 256)
                            yasm_internal_error(N_("Unexpected character token in parameter expansion"));
                        else
                            yasm_error_set(YASM_ERROR_SYNTAX, N_("Cannot handle preprocessor items inside possible macro invocation"));
                }
            }

            /* Now we have the argument count; let's see if it exists */
            mungename = yasm_xmalloc(strlen(name)+8);
            sprintf(mungename, "%s(%d)", name, argc);
            ym = yapp_macro_get(mungename);
            if (!ym)
            {
                ydebug(("YAPP: -Didn't find macro %s\n", mungename));
                replay_saved_tokens(name, &replay_head, to_head, to_tail);
                yasm_xfree(mungename);
                return;
            }
            ydebug(("YAPP: +Found macro %s\n", mungename));

            ym->expanding = 1;

            /* so the macro exists. build a HAMT parameter table */
            param_table = HAMT_create(yasm_internal_error_);
            /* fill the entries by walking the replay buffer and create
             * "macros".  coincidentally, clear the replay buffer. */

            /* get to the opening paren */
            replay = SLIST_FIRST(&replay_head);
            while (replay->token.type != '(') {
                ydebug(("YAPP: Ignoring replay token '%c' \"%s\"\n", replay->token.type, replay->token.str));
                SLIST_REMOVE_HEAD(&replay_head, next);
                yasm_xfree(replay->token.str);
                yasm_xfree(replay);
                replay = SLIST_FIRST(&replay_head);
            }
            ydebug(("YAPP: Ignoring replay token '%c' \"%s\"\n", replay->token.type, replay->token.str));

            /* free the open paren */
            SLIST_REMOVE_HEAD(&replay_head, next);
            yasm_xfree(replay->token.str);
            yasm_xfree(replay);

            param = SLIST_FIRST(&ym->param_head);

            SLIST_INIT(&arg_head);
            arg_tail = SLIST_FIRST(&arg_head);

            replay = SLIST_FIRST(&replay_head);
            SLIST_REMOVE_HEAD(&replay_head, next);
            while (parennest || replay) {
                if (replay->token.type == '(') {
                    parennest++;
                    append_processed_token(replay, &arg_head, &arg_tail);
                    ydebug(("YAPP: +add arg token '%c'\n", replay->token.type));
                }
                else if (parennest && replay->token.type == ')') {
                    parennest--;
                    append_processed_token(replay, &arg_head, &arg_tail);
                    ydebug(("YAPP: +add arg token '%c'\n", replay->token.type));
                }
                else if ((!parennest) && (replay->token.type == ','
                                          || replay->token.type == ')'))
                {
                    int zero=0;
                    struct source_head *argmacro = yasm_xmalloc(sizeof(struct source_head));
                    memcpy(argmacro, &arg_head, sizeof(struct source_head));
                    SLIST_INIT(&arg_head);
                    arg_tail = SLIST_FIRST(&arg_head);

                    /* don't save the comma */
                    yasm_xfree(replay->token.str);
                    yasm_xfree(replay);

                    HAMT_insert(param_table,
                                param->token.str,
                                (void *)argmacro,
                                &zero,
                                (void (*)(void *))yapp_macro_error_sameargname);
                    param = SLIST_NEXT(param, next);
                }
                else if (replay->token.type == IDENT
                         && yapp_defined(replay->token.str))
                {
                    ydebug(("YAPP: +add arg macro '%c' \"%s\"\n", replay->token.type, replay->token.str));
                    expand_macro(replay->token.str, &replay_head, &arg_head, &arg_tail);
                }
                else if (arg_tail || replay->token.type != WHITESPACE) {
                    ydebug(("YAPP: +else add arg token '%c' \"%s\"\n", replay->token.type, replay->token.str));
                    append_processed_token(replay, &arg_head, &arg_tail);
                }
                replay = SLIST_FIRST(&replay_head);
                if (replay) SLIST_REMOVE_HEAD(&replay_head, next);
            }
            if (replay) {
                yasm_xfree(replay->token.str);
                yasm_xfree(replay);
            }
            else if (param) {
                  yasm_internal_error(N_("Got to end of arglist before end of replay!"));
            }
            replay = SLIST_FIRST(&replay_head);
            if (replay || param)
                yasm_internal_error(N_("Count and distribution of define args mismatched!"));

            /* the param_table is set up without errors, so expansion is ready
             * to go */
            SLIST_FOREACH (replay, &ym->macro_head, next) {
                if (replay->token.type == IDENT) {

                    /* check local args first */
                    struct source_head *paramexp =
                        yapp_define_param_get(param_table, replay->token.str);

                    if (paramexp) {
                        expand_token_list(paramexp, to_head, to_tail);
                    }
                    else {
                        /* otherwise, check macros */
                        YAPP_Macro *imacro = yapp_macro_get(replay->token.str);
                        if (imacro != NULL && !imacro->expanding) {
                            expand_macro(replay->token.str, NULL, to_head, to_tail);
                        }
                        else {
                            /* otherwise it's just a vanilla ident */
                            copy_token(&replay->token, to_head, to_tail);
                        }
                    }
                }
                else {
                    copy_token(&replay->token, to_head, to_tail);
                }
            }
            ym->expanding = 0;
        }
    }
    else
        yasm_internal_error(N_("Invoking Macros not yet supported"));

    ym->expanding = 0;
}

void
expand_token_list(struct source_head *paramexp, struct source_head *to_head, source **to_tail)
{
    source *item;
    SLIST_FOREACH (item, paramexp, next) {
        if (item->token.type == IDENT) {
            YAPP_Macro *imacro = yapp_macro_get(item->token.str);
            if (imacro != NULL && !imacro->expanding) {
                expand_macro(item->token.str, NULL, to_head, to_tail);
            }
            else {
                copy_token(&item->token, to_head, to_tail);
            }
        }
        else {
            copy_token(&item->token, to_head, to_tail);
        }
    }
}

static size_t
yapp_preproc_input(yasm_preproc *preproc, char *buf, size_t max_size)
{
    static YAPP_State state = YAPP_STATE_INITIAL;
    size_t n = 0;
    int token;
    int need_line_directive = 0;

    while ((size_t)saved_length < max_size && state != YAPP_STATE_EOF)
    {
        token = yapp_preproc_lex();

        switch (state) {
            case YAPP_STATE_INITIAL:
                switch (token)
                {
                    char *s;
                    default:
                        append_token(token, &source_head, &source_tail);
                        /*if (append_to_return()==0) state=YAPP_STATE_EOF;*/
                        ydebug(("YAPP: default: '%c' \"%s\"\n", token, yapp_preproc_lval.str_val));
                        /*yasm_error_set(YASM_ERROR_SYNTAX, N_("YAPP got an unhandled token."));*/
                        break;

                    case IDENT:
                        ydebug(("YAPP: ident: \"%s\"\n", yapp_preproc_lval.str_val));
                        if (yapp_defined(yapp_preproc_lval.str_val)) {
                            expand_macro(yapp_preproc_lval.str_val, NULL, &source_head, &source_tail);
                        }
                        else {
                            append_token(token, &source_head, &source_tail);
                        }
                        break;

                    case 0:
                        state = YAPP_STATE_EOF;
                        break;

                    case '\n':
                        append_token(token, &source_head, &source_tail);
                        break;

                    case CLEAR:
                        HAMT_destroy(macro_table, (void (*)(void *))yapp_macro_delete);
                        macro_table = HAMT_create(yasm_internal_error_);
                        break;

                    case DEFINE:
                        ydebug(("YAPP: define: "));
                        token = yapp_get_ident("define");
                            ydebug((" \"%s\"\n", yapp_preproc_lval.str_val));
                        s = yasm__xstrdup(yapp_preproc_lval.str_val);

                        /* three cases: newline or stuff or left paren */
                        token = yapp_preproc_lex();
                        if (token == '\n') {
                            /* no args or content - just insert it */
                            yapp_define_insert(s, -1, 0);
                            append_token('\n', &source_head, &source_tail);
                        }
                        else if (token == WHITESPACE) {
                            /* no parens */
                            if(append_to_return(&macro_head, &macro_tail)==0) state=YAPP_STATE_EOF;
                            else {
                                yapp_define_insert(s, -1, 0);
                            }
                            append_token('\n', &source_head, &source_tail);
                        }
                        else if (token == '(') {
                            /* get all params of the parameter list */
                            /* ensure they alternate IDENT and ',' */
                            int param_count = 0;
                            int last_token = ',';

                            ydebug((" *Getting arglist for define %s\n", s));

                            while ((token = yapp_preproc_lex())!=')') {
                                ydebug(("YAPP: +read token '%c' \"%s\" for macro %s\n", token, yapp_preproc_lval.str_val, s));
                                if (token == WHITESPACE) {
                                    token = last_token;
                                }
                                else if (last_token == ',' && token == IDENT) {
                                    append_token(token, &param_head, &param_tail);
                                    param_count++;
                                }
                                else if (token == 0) {
                                    state = YAPP_STATE_EOF;
                                    break;
                                }
                                else if (last_token == ',' || token != ',')
                                    yasm_error_set(YASM_ERROR_SYNTAX, N_("Unexpected token in %%define parameters"));
                                last_token = token;
                            }
                            if (token == ')') {
                                /* after paramlist and ')' */
                                /* everything is what it's defined to be */
                                token = yapp_preproc_lex();
                                if (token != WHITESPACE) append_token(token, &macro_head, &macro_tail);
                                if (append_to_return(&macro_head, &macro_tail)==0) state=YAPP_STATE_EOF;
                                else {
                                    ydebug(("YAPP: Inserting define macro %s (%d)\n", s, param_count));
                                    yapp_define_insert(s, param_count, 0);
                                }
                            }
                            append_token('\n', &source_head, &source_tail);
                        }
                        else {
                            yasm_internal_error(N_("%%define ... failed miserably - neither \\n, WS, or ( followed ident"));
                        }
                        break;

                    case UNDEF:
                        token = yapp_get_ident("undef");
                        yapp_undef(yapp_preproc_lval.str_val);
                        state = YAPP_STATE_NEED_EOL;
                        break;

                    case IFDEF:
                        token = yapp_get_ident("ifdef");
                        push_if(yapp_defined(yapp_preproc_lval.str_val));
                        state = YAPP_STATE_NEED_EOL;
                        break;

                    case IFNDEF:
                        token = yapp_get_ident("ifndef");
                        push_if(!yapp_defined(yapp_preproc_lval.str_val));
                        state = YAPP_STATE_NEED_EOL;
                        break;

                    case ELSE:
                        push_else(1);
                        if (current_output == YAPP_OUTPUT) need_line_directive = 1;
                        state = YAPP_STATE_NEED_EOL;
                        break;

                    case ELIFDEF:
                        token = yapp_get_ident("elifdef");
                        push_else(yapp_defined(yapp_preproc_lval.str_val));
                        if (current_output == YAPP_OUTPUT) need_line_directive = 1;
                        state = YAPP_STATE_NEED_EOL;
                        break;

                    case ELIFNDEF:
                        token = yapp_get_ident("elifndef");
                        push_else(!yapp_defined(yapp_preproc_lval.str_val));
                        if (current_output == YAPP_OUTPUT) need_line_directive = 1;
                        state = YAPP_STATE_NEED_EOL;
                        break;

                    case ENDIF:
                        /* there's got to be another way to do this:   */
                        /* only set if going from non-output to output */
                        if (current_output != YAPP_OUTPUT) need_line_directive = 1;
                        pop_if();
                        if (current_output != YAPP_OUTPUT) need_line_directive = 0;
                        state = YAPP_STATE_NEED_EOL;
                        break;

                    case INCLUDE:
                    case LINE:
                        need_line_directive = 1;
                        break;
                }
                if (state == YAPP_STATE_NEED_EOL)
                {
                    if (eat_through_return(&source_head, &source_tail)==0) state=YAPP_STATE_EOF;
                    else state=YAPP_STATE_INITIAL;
                }
                break;
            default:
                yasm_error_set(YASM_ERROR_PARSE, N_("YAPP got into a bad state"));
        }
        if (need_line_directive) {
            append_token(LINE, &source_head, &source_tail);
            need_line_directive = 0;
        }
    }

    /* convert saved stuff into output.  we either have enough, or are EOF */
    while (n < max_size && saved_length)
    {
        src = SLIST_FIRST(&source_head);
        if (max_size - n >= strlen(src->token.str)) {
            strcpy(buf+n, src->token.str);
            n += strlen(src->token.str);

            saved_length -= strlen(src->token.str);
            SLIST_REMOVE_HEAD(&source_head, next);
            yasm_xfree(src->token.str);
            yasm_xfree(src);
        }
    }

    return n;
}

static size_t
yapp_preproc_get_included_file(yasm_preproc *preproc, /*@out@*/ char *buf,
                               size_t max_size)
{
    /* TODO */
    return 0;
}

static void
yapp_preproc_add_include_file(yasm_preproc *preproc, const char *filename)
{
    /* TODO */
}

static void
yapp_preproc_predefine_macro(yasm_preproc *preproc, const char *macronameval)
{
    /* TODO */
}

static void
yapp_preproc_undefine_macro(yasm_preproc *preproc, const char *macroname)
{
    /* TODO */
}

static void
yapp_preproc_define_builtin(yasm_preproc *preproc, const char *macronameval)
{
    /* TODO */
}

static void
yapp_preproc_add_standard(yasm_preproc *preproc, const char **macros)
{
    /* TODO */
}

/* Define preproc structure -- see preproc.h for details */
yasm_preproc_module yasm_yapp_LTX_preproc = {
    "YAPP preprocessing (NASM style)",
    "yapp",
    yapp_preproc_create,
    yapp_preproc_destroy,
    yapp_preproc_input,
    yapp_preproc_get_included_file,
    yapp_preproc_add_include_file,
    yapp_preproc_predefine_macro,
    yapp_preproc_undefine_macro,
    yapp_preproc_define_builtin,
    yapp_preproc_add_standard
};
