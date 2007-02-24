/* $Id$
 *
 * Generate Arch Parser (GAP): generates ARCHparse.c from ARCHparse.gap.
 *
 *  Copyright (C) 2006  Peter Johnson
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
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include "tools/gap/perfect.h"
#include "libyasm/compat-queue.h"
#include "libyasm/coretype.h"
#include "libyasm/errwarn.h"

typedef STAILQ_HEAD(slist, sval) slist;
typedef struct sval {
    STAILQ_ENTRY(sval) link;
    char *str;
} sval;

typedef STAILQ_HEAD(dir_list, dir) dir_list;
typedef struct dir {
    STAILQ_ENTRY(dir) link;
    char *name;
    const char *func;
    slist args;
} dir;

typedef STAILQ_HEAD(dir_byp_list, dir_byp) dir_byp_list;
typedef struct dir_byp {
    STAILQ_ENTRY(dir_byp) link;
    /*@null@*/ char *parser;
    dir_list dirs;
} dir_byp;

typedef enum {
    ARCH = 0,
    PARSERS,
    INSN,
    CPU,
    CPU_ALIAS,
    CPU_FEATURE,
    TARGETMOD,
    PREFIX,
    REG,
    REGGROUP,
    SEGREG,
    NUM_DIRS
} dir_type;

typedef struct {
    void (*parse_insn) (void);	/* arch-specific parse_insn */
    int multi_parser[NUM_DIRS]; /* whether it has an initial parser field */
} arch_handler;

static void x86_parse_insn(void);
static const arch_handler arch_x86 = {
    x86_parse_insn,
    {0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0}
};

static struct {
    const char *name;
    const arch_handler *arch;
} archs[] = {
    {"x86", &arch_x86},
};

static char line[1024];
static unsigned int cur_line = 0, next_line = 1;
static int errors = 0;
static const arch_handler *arch = NULL;

/* Lists of directives, keyed by parser name */
static dir_byp_list insnprefix_byp;
static dir_byp_list cpu_byp;
static dir_byp_list regtmod_byp;

static void
report_error(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%u: ", cur_line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    errors++;
}

void
yasm__fatal(const char *message, ...)
{
    abort();
}

static void
dup_slist(slist *out, slist *in)
{
    sval *sv;

    STAILQ_INIT(out);
    STAILQ_FOREACH(sv, in, link) {
	sval *nsv = yasm_xmalloc(sizeof(sval));
	nsv->str = yasm__xstrdup(sv->str);
	STAILQ_INSERT_TAIL(out, nsv, link);
    }
}

static dir *
dup_dir(dir *in)
{
    dir *out = yasm_xmalloc(sizeof(dir));
    out->name = yasm__xstrdup(in->name);
    out->func = in->func;
    dup_slist(&out->args, &in->args);
    return out;
}

static dir_list *
get_dirs(dir_byp_list *byp, /*@null@*/ const char *parser)
{
    dir_list *found = NULL;
    dir_byp *db;

    if (STAILQ_EMPTY(byp)) {
	report_error("PARSERS not yet specified");
	return NULL;
    }

    STAILQ_FOREACH(db, byp, link) {
	if ((!parser && !db->parser) ||
	    (parser && db->parser && strcmp(parser, db->parser) == 0)) {
	    found = &db->dirs;
	    break;
	}
    }

    return found;
}

/* Add a keyword/data to a slist of slist keyed by parser name.
 * Returns nonzero on error.
 */
static int
add_dir(dir_byp_list *byp, /*@null@*/ const char *parser, dir *d)
{
    dir_list *found = get_dirs(byp, parser);

    if (found) {
	STAILQ_INSERT_TAIL(found, d, link);
	return 0;
    } else if (!parser) {
	/* Add separately to all */
	dir_byp *db;
	int first = 1;
	STAILQ_FOREACH(db, byp, link) {
	    if (!first)
		d = dup_dir(d);
	    first = 0;
	    STAILQ_INSERT_TAIL(&db->dirs, d, link);
	}
	return 0;
    } else {
	report_error("parser not found");
	return 1;
    }
}

static char *
check_parser(dir_type type)
{
    char *parser = NULL;

    if (arch->multi_parser[type]) {
	parser = strtok(NULL, " \t\n");
	if (strcmp(parser, "-") == 0)
	    parser = NULL;
    }

    return parser;
}

static void
parse_args(slist *args)
{
    char *tok;
    sval *sv;

    STAILQ_INIT(args);

    tok = strtok(NULL, " \t\n");
    if (!tok) {
	report_error("no args");
	return;
    }

    while (tok) {
	sv = yasm_xmalloc(sizeof(sval));
	sv->str = yasm__xstrdup(tok);
	STAILQ_INSERT_TAIL(args, sv, link);
	tok = strtok(NULL, " \t\n");
    }
}

static dir *
parse_generic(dir_type type, const char *func, dir_byp_list *byp)
{
    char *parser = check_parser(type);
    char *name = strtok(NULL, " \t\n");
    dir *d = yasm_xmalloc(sizeof(dir));

    d->name = yasm__xstrdup(name);
    d->func = func;
    parse_args(&d->args);

    add_dir(byp, parser, d);
    return d;
}

static void
parse_arch(void)
{
    size_t i;
    int found = 0;
    char *tok = strtok(NULL, " \t\n");

    if (!tok) {
	report_error("ARCH requires an operand");
	return;
    }
    for (i=0; i<sizeof(archs)/sizeof(archs[0]); i++) {
	if (strcmp(archs[i].name, tok) == 0) {
	    found = 1;
	    break;
	}
    }
    if (!found) {
	report_error("unrecognized ARCH");
	return;
    }

    arch = archs[i].arch;
}

static void
parse_parsers(void)
{
    dir_byp *db;
    char *tok;

    if (!arch) {
	report_error("ARCH not specified before PARSERS");
	return;
    }

    tok = strtok(NULL, " \t\n");
    if (!tok) {
	report_error("no PARSERS parameter");
	return;
    }

    while (tok) {
	/* Insert into each slist of slist if broken out by parser */
	if (arch->multi_parser[INSN] || arch->multi_parser[PREFIX]) {
	    db = yasm_xmalloc(sizeof(dir_byp));
	    db->parser = yasm__xstrdup(tok);
	    STAILQ_INIT(&db->dirs);

	    STAILQ_INSERT_TAIL(&insnprefix_byp, db, link);
	}
	if (arch->multi_parser[CPU] || arch->multi_parser[CPU_ALIAS] ||
	    arch->multi_parser[CPU_FEATURE]) {
	    db = yasm_xmalloc(sizeof(dir_byp));
	    db->parser = yasm__xstrdup(tok);
	    STAILQ_INIT(&db->dirs);

	    STAILQ_INSERT_TAIL(&cpu_byp, db, link);
	}
	if (arch->multi_parser[TARGETMOD] || arch->multi_parser[REG] ||
	    arch->multi_parser[REGGROUP] || arch->multi_parser[SEGREG]) {
	    db = yasm_xmalloc(sizeof(dir_byp));
	    db->parser = yasm__xstrdup(tok);
	    STAILQ_INIT(&db->dirs);

	    STAILQ_INSERT_TAIL(&regtmod_byp, db, link);
	}
	tok = strtok(NULL, " \t\n");
    }

    /* Add NULL (global) versions if not already created */
    if (STAILQ_EMPTY(&insnprefix_byp)) {
	db = yasm_xmalloc(sizeof(dir_byp));
	db->parser = NULL;
	STAILQ_INIT(&db->dirs);

	STAILQ_INSERT_TAIL(&insnprefix_byp, db, link);
    }
    if (STAILQ_EMPTY(&cpu_byp)) {
	db = yasm_xmalloc(sizeof(dir_byp));
	db->parser = NULL;
	STAILQ_INIT(&db->dirs);

	STAILQ_INSERT_TAIL(&cpu_byp, db, link);
    }
    if (STAILQ_EMPTY(&regtmod_byp)) {
	db = yasm_xmalloc(sizeof(dir_byp));
	db->parser = NULL;
	STAILQ_INIT(&db->dirs);

	STAILQ_INSERT_TAIL(&regtmod_byp, db, link);
    }
}

static void
x86_parse_insn(void)
{
    char *parser = check_parser(INSN);
    char *bname = strtok(NULL, " \t\n");
    char *suffix = strtok(NULL, " \t\n");
    dir *d;
    slist args;
    sval *sv;

    if (!suffix) {
	report_error("INSN requires suffix");
	return;
    }

    /* save the remainder of args */
    parse_args(&args);

    if (suffix[0] != '"') {
	/* Just one instruction to generate */
	sv = yasm_xmalloc(sizeof(sval));
	sv->str = yasm__xstrdup(suffix);
	STAILQ_INSERT_HEAD(&args, sv, link);

	d = yasm_xmalloc(sizeof(dir));
	d->name = yasm__xstrdup(bname);
	d->func = "INSN";
	d->args = args;
	add_dir(&insnprefix_byp, parser, d);
    } else {
	/* Need to generate with suffixes for gas */
	char *p;
	char sufstr[6];
	size_t bnamelen = strlen(bname);

	strcpy(sufstr, "SUF_X");

	for (p = &suffix[1]; *p != '"'; p++) {
	    sufstr[4] = toupper(*p);

	    d = yasm_xmalloc(sizeof(dir));

	    d->name = yasm_xmalloc(bnamelen+2);
	    strcpy(d->name, bname);
	    d->name[bnamelen] = tolower(*p);
	    d->name[bnamelen+1] = '\0';

	    d->func = "INSN";
	    dup_slist(&d->args, &args);

	    sv = yasm_xmalloc(sizeof(sval));
	    sv->str = yasm__xstrdup(sufstr);
	    STAILQ_INSERT_HEAD(&d->args, sv, link);

	    add_dir(&insnprefix_byp, "gas", d);
	}

	/* And finally the version sans suffix */
	sv = yasm_xmalloc(sizeof(sval));
	sv->str = yasm__xstrdup("NONE");
	STAILQ_INSERT_HEAD(&args, sv, link);

	d = yasm_xmalloc(sizeof(dir));
	d->name = yasm__xstrdup(bname);
	d->func = "INSN";
	d->args = args;
	add_dir(&insnprefix_byp, parser, d);
    }
}

static void
parse_insn(void)
{
    if (!arch) {
	report_error("ARCH not defined prior to INSN");
	return;
    }
    arch->parse_insn();
}

static void
parse_cpu(void)
{
    dir *d = parse_generic(CPU, "CPU", &cpu_byp);
    sval *sv = yasm_xmalloc(sizeof(sval));
    sv->str = yasm__xstrdup("CPU_MODE_VERBATIM");
    STAILQ_INSERT_TAIL(&d->args, sv, link);
}

static void
parse_cpu_alias(void)
{
    char *parser = check_parser(CPU_ALIAS);
    char *name = strtok(NULL, " \t\n");
    char *alias = strtok(NULL, " \t\n");
    dir_list *dirs = get_dirs(&cpu_byp, parser);
    dir *aliasd, *d;

    if (!alias) {
	report_error("CPU_ALIAS requires an operand");
	return;
    }

    STAILQ_FOREACH(aliasd, dirs, link) {
	if (strcmp(aliasd->name, alias) == 0)
	    break;
    }
    if (!aliasd) {
	report_error("could not find `%s'", alias);
	return;
    }

    d = yasm_xmalloc(sizeof(dir));
    d->name = yasm__xstrdup(name);
    d->func = "CPU";
    dup_slist(&d->args, &aliasd->args);

    add_dir(&cpu_byp, parser, d);
}

static void
parse_cpu_feature(void)
{
    char *parser = check_parser(CPU_FEATURE);
    char *name = strtok(NULL, " \t\n");
    dir *name_dir = yasm_xmalloc(sizeof(dir));
    dir *noname_dir = yasm_xmalloc(sizeof(dir));
    sval *sv;

    name_dir->name = yasm__xstrdup(name);
    name_dir->func = "CPU_FEATURE";
    parse_args(&name_dir->args);

    noname_dir->name = yasm_xmalloc(strlen(name)+3);
    strcpy(noname_dir->name, "no");
    strcat(noname_dir->name, name);
    noname_dir->func = name_dir->func;
    dup_slist(&noname_dir->args, &name_dir->args);

    sv = yasm_xmalloc(sizeof(sval));
    sv->str = yasm__xstrdup("CPU_MODE_SET");
    STAILQ_INSERT_TAIL(&name_dir->args, sv, link);

    sv = yasm_xmalloc(sizeof(sval));
    sv->str = yasm__xstrdup("CPU_MODE_CLEAR");
    STAILQ_INSERT_TAIL(&noname_dir->args, sv, link);

    add_dir(&cpu_byp, parser, name_dir);
    add_dir(&cpu_byp, parser, noname_dir);
}

static void
parse_targetmod(void)
{
    parse_generic(TARGETMOD, "TARGETMOD", &regtmod_byp);
}

static void
parse_prefix(void)
{
    parse_generic(PREFIX, "PREFIX", &insnprefix_byp);
}

static void
parse_reg(void)
{
    parse_generic(REG, "REG", &regtmod_byp);
}

static void
parse_reggroup(void)
{
    parse_generic(REGGROUP, "REGGROUP", &regtmod_byp);
}

static void
parse_segreg(void)
{
    parse_generic(SEGREG, "SEGREG", &regtmod_byp);
}

/* make the c output for the perfect hash tab array */
static void
make_c_tab(
    FILE *f,
    const char *which,
    const char *parser,
    bstuff *tab,	/* table indexed by b */
    ub4 smax,		/* range of scramble[] */
    ub4 blen,		/* b in 0..blen-1, power of 2 */
    ub4 *scramble)	/* used in final hash */
{
    ub4   i;
    /* table for the mapping for the perfect hash */
    if (blen >= USE_SCRAMBLE) {
	/* A way to make the 1-byte values in tab bigger */
	if (smax > UB2MAXVAL+1) {
	    fprintf(f, "static const unsigned long %s_", which);
	    if (parser)
		fprintf(f, "%s_", parser);
	    fprintf(f, "scramble[] = {\n");
	    for (i=0; i<=UB1MAXVAL; i+=4)
		fprintf(f, "0x%.8lx, 0x%.8lx, 0x%.8lx, 0x%.8lx,\n",
		    scramble[i+0], scramble[i+1], scramble[i+2], scramble[i+3]);
	} else {
	    fprintf(f, "static const unsigned short %s_", which);
	    if (parser)
		fprintf(f, "%s_", parser);
	    fprintf(f, "scramble[] = {\n");
	    for (i=0; i<=UB1MAXVAL; i+=8)
		fprintf(f, 
"0x%.4lx, 0x%.4lx, 0x%.4lx, 0x%.4lx, 0x%.4lx, 0x%.4lx, 0x%.4lx, 0x%.4lx,\n",
		    scramble[i+0], scramble[i+1], scramble[i+2], scramble[i+3],
		    scramble[i+4], scramble[i+5], scramble[i+6], scramble[i+7]);
	}
	fprintf(f, "};\n");
	fprintf(f, "\n");
    }

    if (blen > 0) {
	/* small adjustments to _a_ to make values distinct */
	if (smax <= UB1MAXVAL+1 || blen >= USE_SCRAMBLE)
	    fprintf(f, "static const unsigned char %s_", which);
	else
	    fprintf(f, "static const unsigned short %s_", which);
	if (parser)
	    fprintf(f, "%s_", parser);
	fprintf(f, "tab[] = {\n");

	if (blen < 16) {
	    for (i=0; i<blen; ++i)
		fprintf(f, "%3ld,", scramble[tab[i].val_b]);
	} else if (blen <= 1024) {
	    for (i=0; i<blen; i+=16)
		fprintf(f, "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,\n",
		    scramble[tab[i+0].val_b], scramble[tab[i+1].val_b], 
		    scramble[tab[i+2].val_b], scramble[tab[i+3].val_b], 
		    scramble[tab[i+4].val_b], scramble[tab[i+5].val_b], 
		    scramble[tab[i+6].val_b], scramble[tab[i+7].val_b], 
		    scramble[tab[i+8].val_b], scramble[tab[i+9].val_b], 
		    scramble[tab[i+10].val_b], scramble[tab[i+11].val_b], 
		    scramble[tab[i+12].val_b], scramble[tab[i+13].val_b], 
		    scramble[tab[i+14].val_b], scramble[tab[i+15].val_b]); 
	} else if (blen < USE_SCRAMBLE) {
	    for (i=0; i<blen; i+=8)
		fprintf(f, "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,\n",
		    scramble[tab[i+0].val_b], scramble[tab[i+1].val_b], 
		    scramble[tab[i+2].val_b], scramble[tab[i+3].val_b], 
		    scramble[tab[i+4].val_b], scramble[tab[i+5].val_b], 
		    scramble[tab[i+6].val_b], scramble[tab[i+7].val_b]); 
	} else {
	    for (i=0; i<blen; i+=16)
		fprintf(f, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
		    tab[i+0].val_b, tab[i+1].val_b, 
		    tab[i+2].val_b, tab[i+3].val_b, 
		    tab[i+4].val_b, tab[i+5].val_b, 
		    tab[i+6].val_b, tab[i+7].val_b, 
		    tab[i+8].val_b, tab[i+9].val_b, 
		    tab[i+10].val_b, tab[i+11].val_b, 
		    tab[i+12].val_b, tab[i+13].val_b, 
		    tab[i+14].val_b, tab[i+15].val_b); 
	}
	fprintf(f, "};\n");
	fprintf(f, "\n");
    }
}

static void
perfect_dir(FILE *out, const char *which, const char *parser, dir_list *dirs)
{
    ub4 nkeys;
    key *keys;
    hashform form;
    bstuff *tab;		/* table indexed by b */
    hstuff *tabh;		/* table indexed by hash value */
    ub4 smax;		/* scramble[] values in 0..smax-1, a power of 2 */
    ub4 alen;			/* a in 0..alen-1, a power of 2 */
    ub4 blen;			/* b in 0..blen-1, a power of 2 */
    ub4 salt;			/* a parameter to the hash function */
    gencode final;		/* code for final hash */
    ub4 i;
    ub4 scramble[SCRAMBLE_LEN];	/* used in final hash function */
    char buf[10][80];		/* buffer for generated code */
    char *buf2[10];		/* also for generated code */
    int cpumode = strcmp(which, "cpu") == 0;
    dir *d;

    /* perfect hash configuration */
    form.mode = NORMAL_HM;
    form.hashtype = STRING_HT;
    form.perfect = MINIMAL_HP;
    form.speed = SLOW_HS;

    /* set up code for final hash */
    final.line = buf2;
    final.used = 0;
    final.len  = 10;
    for (i=0; i<10; i++)
	final.line[i] = buf[i];

    /* build list of keys */
    nkeys = 0;
    keys = NULL;
    STAILQ_FOREACH(d, dirs, link) {
	key *k = yasm_xmalloc(sizeof(key));

	k->name_k = yasm__xstrdup(d->name);
	k->len_k = (ub4)strlen(d->name);
	k->next_k = keys;
	keys = k;
	nkeys++;
    }

    /* find the hash */
    findhash(&tab, &tabh, &alen, &blen, &salt, &final, 
	     scramble, &smax, keys, nkeys, &form);

    /* output the dir table: this should loop up to smax for NORMAL_HP,
     * or up to pakd.nkeys for MINIMAL_HP.
     */
    fprintf(out, "static const %s_parse_data %s_", which, which);
    if (parser)
	fprintf(out, "%s_", parser);
    fprintf(out, "pd[%lu] = {\n", nkeys);
    for (i=0; i<nkeys; i++) {
	if (tabh[i].key_h) {
	    sval *sv;
	    STAILQ_FOREACH(d, dirs, link) {
		if (strcmp(d->name, tabh[i].key_h->name_k) == 0)
		    break;
	    }
	    if (!d) {
		report_error("internal error: could not find `%s'",
			     tabh[i].key_h->name_k);
		break;
	    }
	    if (cpumode)
		fprintf(out, "{\"%s\",", d->name);
	    else
		fprintf(out, "%s(\"%s\",", d->func, d->name);
	    STAILQ_FOREACH(sv, &d->args, link) {
		fprintf(out, " %s", sv->str);
		if (STAILQ_NEXT(sv, link))
		    fprintf(out, ",");
	    }
	    fprintf(out, cpumode ? "}" : ")");
	} else
	    fprintf(out, "  { NULL }");

	if (i < nkeys-1)
	    fprintf(out, ",");
	fprintf(out, "\n");
    }
    fprintf(out, "};\n");

    /* output the hash tab[] array */
    make_c_tab(out, which, parser, tab, smax, blen, scramble);

    /* The hash function */
    fprintf(out, "#define tab %s_", which);
    if (parser)
	fprintf(out, "%s_", parser);
    fprintf(out, "tab\n");
    fprintf(out, "static const %s_parse_data *\n%s_", which, which);
    if (parser)
	fprintf(out, "%s_", parser);
    fprintf(out, "find(const char *key, size_t len)\n");
    fprintf(out, "{\n");
    fprintf(out, "  const %s_parse_data *ret;\n", which);
    for (i=0; i<final.used; ++i)
	fprintf(out, final.line[i]);
    fprintf(out, "  if (rsl >= %lu) return NULL;\n", nkeys);
    fprintf(out, "  ret = &%s_", which);
    if (parser)
	fprintf(out, "%s_", parser);
    fprintf(out, "pd[rsl];\n");
    fprintf(out, "  if (strcmp(key, ret->name) != 0) return NULL;\n");
    fprintf(out, "  return ret;\n");
    fprintf(out, "}\n");
    fprintf(out, "#undef tab\n\n");

    free(tab);
    free(tabh);
}

/* Get an entire "real" line from the input file by combining any
 * \\\n continuations.
 */
static int get_line(FILE *in)
{
    char *p = line;
    cur_line = next_line;

    if (feof(in))
	return 0;

    while (p < &line[1023-128]) {
	if (!fgets(p, 128, in))
	    return 1;
	next_line++;
	/* if continuation, strip out leading whitespace */
	if (p > line) {
	    char *p2 = p;
	    while (isspace(*p2)) p2++;
	    if (p2 > p)
		memmove(p, p2, strlen(p2)+1);
	}
	while (*p) p++;
	if (p[-2] != '\\' || p[-1] != '\n') {
	    if (p[-1] == '\n')
		p[-1] = '\0';
	    return 1;
	}
	p -= 2;
    }
    return 0;
}

static struct {
    const char *name;
    int indx;
    void (*handler) (void);
} directives[] = {
    {"ARCH", ARCH, parse_arch},
    {"PARSERS", PARSERS, parse_parsers},
    {"INSN", INSN, parse_insn},
    {"CPU", CPU, parse_cpu},
    {"CPU_ALIAS", CPU_ALIAS, parse_cpu_alias},
    {"CPU_FEATURE", CPU_FEATURE, parse_cpu_feature},
    {"TARGETMOD", TARGETMOD, parse_targetmod},
    {"PREFIX", PREFIX, parse_prefix},
    {"REG", REG, parse_reg},
    {"REGGROUP", REGGROUP, parse_reggroup},
    {"SEGREG", SEGREG, parse_segreg},
};

int
main(int argc, char *argv[])
{
    FILE *in, *out;
    size_t i;
    char *tok;
    int count[NUM_DIRS];
    dir_byp *db;

    for (i=0; i<NUM_DIRS; i++)
	count[i] = 0;

    if (argc != 3) {
	fprintf(stderr, "Usage: gap <in> <out>\n");
	return EXIT_FAILURE;
    }

    in = fopen(argv[1], "rt");
    if (!in) {
	fprintf(stderr, "Could not open `%s' for reading\n", argv[1]);
	return EXIT_FAILURE;
    }

    STAILQ_INIT(&insnprefix_byp);
    STAILQ_INIT(&cpu_byp);
    STAILQ_INIT(&regtmod_byp);

    /* Parse input file */
    while (get_line(in)) {
	int found;
	/*printf("%s\n", line);*/
	tok = strtok(line, " \t\n");
	if (!tok)
	    continue;

	/* Comments start with # as the first thing on a line */
	if (tok[0] == '#')
	    continue;

	/* Look for directive */
	found = 0;
	for (i=0; i<sizeof(directives)/sizeof(directives[0]); i++) {
	    if (strcmp(tok, directives[i].name) == 0) {
		count[directives[i].indx]++;
		directives[i].handler();
		found = 1;
		break;
	    }
	}
	if (!found)
	    report_error("unknown directive `%s'\n", tok);
    }

    /* Output some informational statistics */
    printf("Directives read:\n");
    for (i=0; i<sizeof(directives)/sizeof(directives[0]); i++)
	printf("\t%d\t%s\n", count[directives[i].indx], directives[i].name);

    if (errors > 0)
	return EXIT_FAILURE;

    out = fopen(argv[2], "wt");
    if (!out) {
	fprintf(stderr, "Could not open `%s' for writing\n", argv[2]);
	return EXIT_FAILURE;
    }

    /* Get perfect hashes for the three lists of directives */
    STAILQ_FOREACH(db, &insnprefix_byp, link)
	perfect_dir(out, "insnprefix", db->parser, &db->dirs);
    STAILQ_FOREACH(db, &cpu_byp, link)
	perfect_dir(out, "cpu", db->parser, &db->dirs);
    STAILQ_FOREACH(db, &regtmod_byp, link)
	perfect_dir(out, "regtmod", db->parser, &db->dirs);

    if (errors > 0)
	return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

