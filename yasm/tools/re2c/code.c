#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tools/re2c/substr.h"
#include "tools/re2c/globals.h"
#include "tools/re2c/dfa.h"

/* there must be at least one span in list;  all spans must cover
 * same range
 */

void Go_compact(Go *g){
    /* arrange so that adjacent spans have different targets */
    unsigned int i = 0, j;
    for(j = 1; j < g->nSpans; ++j){
	if(g->span[j].to != g->span[i].to){
	    ++i; g->span[i].to = g->span[j].to;
	}
	g->span[i].ub = g->span[j].ub;
    }
    g->nSpans = i + 1;
}

void Go_unmap(Go *g, Go *base, State *x){
    Span *s = g->span, *b = base->span, *e = &b[base->nSpans];
    unsigned int lb = 0;
    s->ub = 0;
    s->to = NULL;
    for(; b != e; ++b){
	if(b->to == x){
	    if((s->ub - lb) > 1)
		s->ub = b->ub;
	} else {
	    if(b->to != s->to){
		if(s->ub){
		    lb = s->ub; ++s;
		}
		s->to = b->to;
	    }
	    s->ub = b->ub;
	}
    }
    s->ub = e[-1].ub; ++s;
    g->nSpans = s - g->span;
}

static void doGen(Go *g, State *s, unsigned char *bm, unsigned char m){
    Span *b = g->span, *e = &b[g->nSpans];
    unsigned int lb = 0;
    for(; b < e; ++b){
	if(b->to == s)
	    for(; lb < b->ub; ++lb) bm[lb] |= m;
	lb = b->ub;
    }
}
#if 0
static void prt(FILE *o, Go *g, State *s){
    Span *b = g->span, *e = &b[g->nSpans];
    unsigned int lb = 0;
    for(; b < e; ++b){
	if(b->to == s)
	    printSpan(o, lb, b->ub);
	lb = b->ub;
    }
}
#endif
static int matches(Go *g1, State *s1, Go *g2, State *s2){
    Span *b1 = g1->span, *e1 = &b1[g1->nSpans];
    unsigned int lb1 = 0;
    Span *b2 = g2->span, *e2 = &b2[g2->nSpans];
    unsigned int lb2 = 0;
    for(;;){
	for(; b1 < e1 && b1->to != s1; ++b1) lb1 = b1->ub;
	for(; b2 < e2 && b2->to != s2; ++b2) lb2 = b2->ub;
	if(b1 == e1) return b2 == e2;
	if(b2 == e2) return 0;
	if(lb1 != lb2 || b1->ub != b2->ub) return 0;
	++b1; ++b2;
    }
}

typedef struct BitMap {
    Go			*go;
    State		*on;
    struct BitMap	*next;
    unsigned int	i;
    unsigned char	m;
} BitMap;

static BitMap *BitMap_find_go(Go*, State*);
static BitMap *BitMap_find(State*);
static void BitMap_gen(FILE *, unsigned int, unsigned int);
/* static void BitMap_stats(void);*/
static BitMap *BitMap_new(Go*, State*);

static BitMap *BitMap_first = NULL;

BitMap *
BitMap_new(Go *g, State *x)
{
    BitMap *b = malloc(sizeof(BitMap));
    b->go = g;
    b->on = x;
    b->next = BitMap_first;
    BitMap_first = b;
    return b;
}

BitMap *
BitMap_find_go(Go *g, State *x){
    BitMap *b;
    for(b = BitMap_first; b; b = b->next){
	if(matches(b->go, b->on, g, x))
	    return b;
    }
    return BitMap_new(g, x);
}

BitMap *
BitMap_find(State *x){
    BitMap *b;
    for(b = BitMap_first; b; b = b->next){
	if(b->on == x)
	    return b;
    }
    return NULL;
}

void BitMap_gen(FILE *o, unsigned int lb, unsigned int ub){
    BitMap *b = BitMap_first;
    if(b){
	unsigned int n = ub - lb;
	unsigned int i;
	unsigned char *bm = malloc(sizeof(unsigned char)*n);
	memset(bm, 0, n);
	fputs("\tstatic unsigned char yybm[] = {", o);
	for(i = 0; b; i += n){
	    unsigned char m;
	    unsigned int j;
	    for(m = 0x80; b && m; b = b->next, m >>= 1){
		b->i = i; b->m = m;
		doGen(b->go, b->on, bm-lb, m);
	    }
	    for(j = 0; j < n; ++j){
		if(j%8 == 0) {fputs("\n\t", o); oline++;}
		fprintf(o, "%3u, ", (unsigned int) bm[j]);
	    }
	}
	fputs("\n\t};\n", o); oline+=2;
    }
}

#if 0
void BitMap_stats(void){
    unsigned int n = 0;
    BitMap *b;
    for(b = BitMap_first; b; b = b->next){
	prt(stderr, b->go, b->on); fputs("\n", stderr);
	++n;
    }
    fprintf(stderr, "%u bitmaps\n", n);
    BitMap_first = NULL;
}
#endif

static void genGoTo(FILE *o, State *to){
    fprintf(o, "\tgoto yy%u;\n", to->label); oline++;
}

static void genIf(FILE *o, const char *cmp, unsigned int v){
    fprintf(o, "\tif(yych %s '", cmp);
    prtCh(o, v);
    fputs("')", o);
}

static void indent(FILE *o, unsigned int i){
    while(i-- > 0)
	fputc('\t', o);
}

static void need(FILE *o, unsigned int n){
    if(n == 1) {
	fputs("\tif(YYLIMIT == YYCURSOR) YYFILL(1);\n", o); oline++;
    } else {
	fprintf(o, "\tif((YYLIMIT - YYCURSOR) < %u) YYFILL(%u);\n", n, n);
	oline++;
    }
    fputs("\tyych = *YYCURSOR;\n", o); oline++;
}

void
Action_emit(Action *a, FILE *o)
{
    int first = 1;
    unsigned int i;
    unsigned int back;

    switch (a->type) {
	case MATCHACT:
	    if(a->state->link){
		fputs("\t++YYCURSOR;\n", o); oline++;
		need(o, a->state->depth);
	    } else {
		fputs("\tyych = *++YYCURSOR;\n", o); oline++;
	    }
	    break;
	case ENTERACT:
	    if(a->state->link){
		fputs("\t++YYCURSOR;\n", o);
		fprintf(o, "yy%u:\n", a->d.label); oline+=2;
		need(o, a->state->depth);
	    } else {
		fputs("\tyych = *++YYCURSOR;\n", o);
		fprintf(o, "yy%u:\n", a->d.label); oline+=2;
	    }
	    break;
	case SAVEMATCHACT:
	    fprintf(o, "\tyyaccept = %u;\n", a->d.selector); oline++;
	    if(a->state->link){
		fputs("\tYYMARKER = ++YYCURSOR;\n", o); oline++;
		need(o, a->state->depth);
	    } else {
		fputs("\tyych = *(YYMARKER = ++YYCURSOR);\n", o); oline++;
	    }
	    break;
	case MOVEACT:
	    break;
	case ACCEPTACT:
	    for(i = 0; i < a->d.Accept.nRules; ++i)
		if(a->d.Accept.saves[i] != ~0u){
		    if(first){
			first = 0;
			fputs("\tYYCURSOR = YYMARKER;\n", o);
			fputs("\tswitch(yyaccept){\n", o); oline+=2;
		    }
		    fprintf(o, "\tcase %u:", a->d.Accept.saves[i]);
		    genGoTo(o, a->d.Accept.rules[i]);
		}
	    if(!first) {
		fputs("\t}\n", o); oline++;
	    }
	    break;
	case RULEACT:
	    back = RegExp_fixedLength(a->d.rule->d.RuleOp.ctx);
	    if(back != ~0u && back > 0u)
		fprintf(o, "\tYYCURSOR -= %u;", back);
	    fprintf(o, "\n"); oline++;
	    line_source(o, a->d.rule->d.RuleOp.code->line);
	    SubStr_out(&a->d.rule->d.RuleOp.code->text, o);
	    fprintf(o, "\n"); oline++;
	    fprintf(o, "#line %u \"re2c-out.c\"\n", ++oline);
	    break;
    }
}

Action *
Action_new_Accept(State *x, unsigned int n, unsigned int *s, State **r)
{
    Action *a = malloc(sizeof(Action));
    a->type = ACCEPTACT;
    a->state = x;
    a->d.Accept.nRules = n;
    a->d.Accept.saves = s;
    a->d.Accept.rules = r;
    x->action = a;
    return a;
}

static void doLinear(FILE *o, unsigned int i, Span *s, unsigned int n,
		     State *next){
    for(;;){
	State *bg = s[0].to;
	while(n >= 3 && s[2].to == bg && (s[1].ub - s[0].ub) == 1){
	    if(s[1].to == next && n == 3){
		indent(o, i); genIf(o, "!=", s[0].ub); genGoTo(o, bg);
		return;
	    } else {
		indent(o, i); genIf(o, "==", s[0].ub); genGoTo(o, s[1].to);
	    }
	    n -= 2; s += 2;
	}
	if(n == 1){
	    if(bg != next){
		indent(o, i); genGoTo(o, s[0].to);
	    }
	    return;
	} else if(n == 2 && bg == next){
	    indent(o, i); genIf(o, ">=", s[0].ub); genGoTo(o, s[1].to);
	    return;
	} else {
	    indent(o, i); genIf(o, "<=", s[0].ub - 1); genGoTo(o, bg);
	    n -= 1; s += 1;
	}
    }
}

void
Go_genLinear(Go *g, FILE *o, State *next){
    doLinear(o, 0, g->span, g->nSpans, next);
}

static void genCases(FILE *o, unsigned int lb, Span *s){
    if(lb < s->ub){
	for(;;){
	    fputs("\tcase '", o); prtCh(o, lb); fputs("':", o);
	    if(++lb == s->ub)
		break;
	    fputs("\n", o); oline++;
	}
    }
}

void
Go_genSwitch(Go *g, FILE *o, State *next){
    if(g->nSpans <= 2){
	Go_genLinear(g, o, next);
    } else {
	State *def = g->span[g->nSpans-1].to;
	Span **sP = malloc(sizeof(Span*)*(g->nSpans-1)), **r, **s, **t;
	unsigned int i;

	t = &sP[0];
	for(i = 0; i < g->nSpans; ++i)
	    if(g->span[i].to != def)
		*(t++) = &g->span[i];

	fputs("\tswitch(yych){\n", o); oline++;
	while(t != &sP[0]){
	    State *to;
	    r = s = &sP[0];
	    if(*s == &g->span[0])
		genCases(o, 0, *s);
	    else
		genCases(o, (*s)[-1].ub, *s);
	    to = (*s)->to;
	    while(++s < t){
		if((*s)->to == to)
		    genCases(o, (*s)[-1].ub, *s);
		else
		    *(r++) = *s;
	    }
	    genGoTo(o, to);
	    t = r;
	}
	fputs("\tdefault:", o);
	genGoTo(o, def);
	fputs("\t}\n", o); oline++;

	free(sP);
    }
}

static void doBinary(FILE *o, unsigned int i, Span *s, unsigned int n,
		     State *next){
    if(n <= 4){
	doLinear(o, i, s, n, next);
    } else {
	unsigned int h = n/2;
	indent(o, i); genIf(o, "<=", s[h-1].ub - 1); fputs("{\n", o); oline++;
	doBinary(o, i+1, &s[0], h, next);
	indent(o, i); fputs("\t} else {\n", o); oline++;
	doBinary(o, i+1, &s[h], n - h, next);
	indent(o, i); fputs("\t}\n", o); oline++;
    }
}

void
Go_genBinary(Go *g, FILE *o, State *next){
    doBinary(o, 0, g->span, g->nSpans, next);
}

void
Go_genBase(Go *g, FILE *o, State *next){
    if(g->nSpans == 0)
	return;
    if(!sFlag){
	Go_genSwitch(g, o, next);
	return;
    }
    if(g->nSpans > 8){
	Span *bot = &g->span[0], *top = &g->span[g->nSpans-1];
	unsigned int util;
	if(bot[0].to == top[0].to){
	    util = (top[-1].ub - bot[0].ub)/(g->nSpans - 2);
	} else {
	    if(bot[0].ub > (top[0].ub - top[-1].ub)){
		util = (top[0].ub - bot[0].ub)/(g->nSpans - 1);
	    } else {
		util = top[-1].ub/(g->nSpans - 1);
	    }
	}
	if(util <= 2){
	    Go_genSwitch(g, o, next);
	    return;
	}
    }
    if(g->nSpans > 5){
	Go_genBinary(g, o, next);
    } else {
	Go_genLinear(g, o, next);
    }
}

void
Go_genGoto(Go *g, FILE *o, State *next){
    unsigned int i;
    if(bFlag){
	for(i = 0; i < g->nSpans; ++i){
	    State *to = g->span[i].to;
	    if(to && to->isBase){
		BitMap *b = BitMap_find(to);
		if(b && matches(b->go, b->on, g, to)){
		    Go go;
		    go.span = malloc(sizeof(Span)*g->nSpans);
		    Go_unmap(&go, g, to);
		    fprintf(o, "\tif(yybm[%u+yych] & %u)", b->i,
			    (unsigned int) b->m);
		    genGoTo(o, to);
		    Go_genBase(&go, o, next);
		    free(go.span);
		    return;
		}
	    }
	}
    }
    Go_genBase(g, o, next);
}

void State_emit(State *s, FILE *o){
    fprintf(o, "yy%u:", s->label);
    Action_emit(s->action, o);
}

static unsigned int merge(Span *x0, State *fg, State *bg){
    Span *x = x0, *f = fg->go.span, *b = bg->go.span;
    unsigned int nf = fg->go.nSpans, nb = bg->go.nSpans;
    State *prev = NULL, *to;
    /* NB: we assume both spans are for same range */
    for(;;){
	if(f->ub == b->ub){
	    to = f->to == b->to? bg : f->to;
	    if(to == prev){
		--x;
	    } else {
		x->to = prev = to;
	    }
	    x->ub = f->ub;
	    ++x; ++f; --nf; ++b; --nb;
	    if(nf == 0 && nb == 0)
		return x - x0;
	}
	while(f->ub < b->ub){
	    to = f->to == b->to? bg : f->to;
	    if(to == prev){
		--x;
	    } else {
		x->to = prev = to;
	    }
	    x->ub = f->ub;
	    ++x; ++f; --nf;
	}
	while(b->ub < f->ub){
	    to = b->to == f->to? bg : f->to;
	    if(to == prev){
		--x;
	    } else {
		x->to = prev = to;
	    }
	    x->ub = b->ub;
	    ++x; ++b; --nb;
	}
    }
}

const unsigned int cInfinity = ~0;

typedef struct SCC {
    State	**top, **stk;
} SCC;

static void SCC_init(SCC*, unsigned int);
static SCC *SCC_new(unsigned int);
static void SCC_destroy(SCC*);
static void SCC_delete(SCC*);
static void SCC_traverse(SCC*, State*);

static inline void
SCC_init(SCC *s, unsigned int size)
{
    s->top = s->stk = malloc(sizeof(State*)*size);
}

static inline SCC *
SCC_new(unsigned int size){
    SCC *s = malloc(sizeof(SCC));
    s->top = s->stk = malloc(sizeof(State*)*size);
    return s;
}

static inline void
SCC_destroy(SCC *s){
    free(s->stk);
}

static inline void
SCC_delete(SCC *s){
    free(s->stk);
    free(s);
}

static void SCC_traverse(SCC *s, State *x){
    unsigned int k, i;

    *s->top = x;
    k = ++s->top - s->stk;
    x->depth = k;
    for(i = 0; i < x->go.nSpans; ++i){
	State *y = x->go.span[i].to;
	if(y){
	    if(y->depth == 0)
		SCC_traverse(s, y);
	    if(y->depth < x->depth)
		x->depth = y->depth;
	}
    }
    if(x->depth == k)
	do {
	    (*--s->top)->depth = cInfinity;
	    (*s->top)->link = x;
	} while(*s->top != x);
}

static unsigned int maxDist(State *s){
    unsigned int mm = 0, i;
    for(i = 0; i < s->go.nSpans; ++i){
	State *t = s->go.span[i].to;
	if(t){
	    unsigned int m = 1;
	    if(!t->link)
		m += maxDist(t);
	    if(m > mm)
		mm = m;
	}
    }
    return mm;
}

static void calcDepth(State *head){
    State *t, *s;
    for(s = head; s; s = s->next){
	if(s->link == s){
	    unsigned int i;
	    for(i = 0; i < s->go.nSpans; ++i){
		t = s->go.span[i].to;
		if(t && t->link == s)
		    goto inSCC;
	    }
	    s->link = NULL;
	} else {
	inSCC:
	    s->depth = maxDist(s);
	}
    }
}
 
void DFA_findSCCs(DFA *d){
    SCC scc;
    State *s;

    SCC_init(&scc, d->nStates);
    for(s = d->head; s; s = s->next){
	s->depth = 0;
	s->link = NULL;
    }

    for(s = d->head; s; s = s->next)
	if(!s->depth)
	    SCC_traverse(&scc, s);

    calcDepth(d->head);

    SCC_destroy(&scc);
}

void DFA_split(DFA *d, State *s){
    State *move = State_new();
    Action_new_Move(move);
    DFA_addState(d, &s->next, move);
    move->link = s->link;
    move->rule = s->rule;
    move->go = s->go;
    s->rule = NULL;
    s->go.nSpans = 1;
    s->go.span = malloc(sizeof(Span));
    s->go.span[0].ub = d->ubChar;
    s->go.span[0].to = move;
}

void DFA_emit(DFA *d, FILE *o){
    static unsigned int label = 0;
    State *s;
    unsigned int i;
    unsigned int nRules = 0;
    unsigned int nSaves = 0;
    unsigned int *saves;
    State **rules;
    State *accept = NULL;
    Span *span;

    DFA_findSCCs(d);
    d->head->link = d->head;
    d->head->depth = maxDist(d->head);

    for(s = d->head; s; s = s->next)
	if(s->rule && s->rule->d.RuleOp.accept >= nRules)
		nRules = s->rule->d.RuleOp.accept + 1;

    saves = malloc(sizeof(unsigned int)*nRules);
    memset(saves, ~0, (nRules)*sizeof(unsigned int));

    /* mark backtracking points */
    for(s = d->head; s; s = s->next){
	RegExp *ignore = NULL;/*RuleOp*/
	if(s->rule){
	    for(i = 0; i < s->go.nSpans; ++i)
		if(s->go.span[i].to && !s->go.span[i].to->rule){
		    free(s->action);
		    if(saves[s->rule->d.RuleOp.accept] == ~0u)
			saves[s->rule->d.RuleOp.accept] = nSaves++;
		    Action_new_Save(s, saves[s->rule->d.RuleOp.accept]);
		    continue;
		}
	    ignore = s->rule;
	}
    }

    /* insert actions */
    rules = malloc(sizeof(State*)*nRules);
    memset(rules, 0, (nRules)*sizeof(State*));
    for(s = d->head; s; s = s->next){
	State *ow;
	if(!s->rule){
	    ow = accept;
	} else {
	    if(!rules[s->rule->d.RuleOp.accept]){
		State *n = State_new();
		Action_new_Rule(n, s->rule);
		rules[s->rule->d.RuleOp.accept] = n;
		DFA_addState(d, &s->next, n);
	    }
	    ow = rules[s->rule->d.RuleOp.accept];
	}
	for(i = 0; i < s->go.nSpans; ++i)
	    if(!s->go.span[i].to){
		if(!ow){
		    ow = accept = State_new();
		    Action_new_Accept(accept, nRules, saves, rules);
		    DFA_addState(d, &s->next, accept);
		}
		s->go.span[i].to = ow;
	    }
    }

    /* split ``base'' states into two parts */
    for(s = d->head; s; s = s->next){
	s->isBase = 0;
	if(s->link){
	    for(i = 0; i < s->go.nSpans; ++i){
		if(s->go.span[i].to == s){
		    s->isBase = 1;
		    DFA_split(d, s);
		    if(bFlag)
			BitMap_find_go(&s->next->go, s);
		    s = s->next;
		    break;
		}
	    }
	}
    }

    /* find ``base'' state, if possible */
    span = malloc(sizeof(Span)*(d->ubChar - d->lbChar));
    for(s = d->head; s; s = s->next){
	if(!s->link){
	    for(i = 0; i < s->go.nSpans; ++i){
		State *to = s->go.span[i].to;
		if(to && to->isBase){
		    unsigned int nSpans;
		    to = to->go.span[0].to;
		    nSpans = merge(span, s, to);
		    if(nSpans < s->go.nSpans){
			free(s->go.span);
			s->go.nSpans = nSpans;
			s->go.span = malloc(sizeof(Span)*nSpans);
			memcpy(s->go.span, span, nSpans*sizeof(Span));
		    }
		    break;
		}
	    }
	}
    }
    free(span);

    free(d->head->action);

    oline++;
    fprintf(o, "\n#line %u \"re2c-out.c\"\n", ++oline);

    fputs("{\n\tYYCTYPE yych;\n\tunsigned int yyaccept;\n", o); oline+=3;

    if(bFlag)
	BitMap_gen(o, d->lbChar, d->ubChar);

    fprintf(o, "\tgoto yy%u;\n", label); oline++;
    Action_new_Enter(d->head, label++);

    for(s = d->head; s; s = s->next)
	s->label = label++;

    for(s = d->head; s; s = s->next){
	State_emit(s, o);
	Go_genGoto(&s->go, o, s->next);
    }
    fputs("}\n", o); oline++;

    BitMap_first = NULL;

    free(saves);
    free(rules);
}
