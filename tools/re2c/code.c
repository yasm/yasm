#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <iomanip.h>
#include "substr.h"
#include "globals.h"
#include "dfa.h"

// there must be at least one span in list;  all spans must cover
// same range

void Go::compact(){
    // arrange so that adjacent spans have different targets
    uint i = 0;
    for(uint j = 1; j < nSpans; ++j){
	if(span[j].to != span[i].to){
	    ++i; span[i].to = span[j].to;
	}
	span[i].ub = span[j].ub;
    }
    nSpans = i + 1;
}

void Go::unmap(Go *base, State *x){
    Span *s = span, *b = base->span, *e = &b[base->nSpans];
    uint lb = 0;
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
    nSpans = s - span;
}

void doGen(Go *g, State *s, uchar *bm, uchar m){
Span *b = g->span, *e = &b[g->nSpans];
uint lb = 0;
for(; b < e; ++b){
    if(b->to == s)
	for(; lb < b->ub; ++lb) bm[lb] |= m;
    lb = b->ub;
}
}

void prt(ostream& o, Go *g, State *s){
Span *b = g->span, *e = &b[g->nSpans];
uint lb = 0;
for(; b < e; ++b){
    if(b->to == s)
	printSpan(o, lb, b->ub);
    lb = b->ub;
}
}

bool matches(Go *g1, State *s1, Go *g2, State *s2){
Span *b1 = g1->span, *e1 = &b1[g1->nSpans];
uint lb1 = 0;
Span *b2 = g2->span, *e2 = &b2[g2->nSpans];
uint lb2 = 0;
for(;;){
    for(; b1 < e1 && b1->to != s1; ++b1) lb1 = b1->ub;
    for(; b2 < e2 && b2->to != s2; ++b2) lb2 = b2->ub;
    if(b1 == e1) return b2 == e2;
    if(b2 == e2) return false;
    if(lb1 != lb2 || b1->ub != b2->ub) return false;
    ++b1; ++b2;
}
}

class BitMap {
public:
static BitMap	*first;
Go			*go;
State		*on;
BitMap		*next;
uint		i;
uchar		m;
public:
static BitMap *find(Go*, State*);
static BitMap *find(State*);
static void gen(ostream&, uint, uint);
static void stats();
BitMap(Go*, State*);
};

BitMap *BitMap::first = NULL;

BitMap::BitMap(Go *g, State *x) : go(g), on(x), next(first) {
first = this;
}

BitMap *BitMap::find(Go *g, State *x){
for(BitMap *b = first; b; b = b->next){
    if(matches(b->go, b->on, g, x))
	    return b;
    }
    return new BitMap(g, x);
}

BitMap *BitMap::find(State *x){
    for(BitMap *b = first; b; b = b->next){
	if(b->on == x)
	    return b;
    }
    return NULL;
}

void BitMap::gen(ostream &o, uint lb, uint ub){
    BitMap *b = first;
    if(b){
	o << "\tstatic unsigned char yybm[] = {";
	uint n = ub - lb;
	uchar *bm = new uchar[n];
	memset(bm, 0, n);
	for(uint i = 0; b; i += n){
	    for(uchar m = 0x80; b && m; b = b->next, m >>= 1){
		b->i = i; b->m = m;
		doGen(b->go, b->on, bm-lb, m);
	    }
	    for(uint j = 0; j < n; ++j){
		if(j%8 == 0) o << "\n\t";
		o << setw(3) << (uint) bm[j] << ", ";
	    }
	}
	o << "\n\t};\n";
    }
}

void BitMap::stats(){
    uint n = 0;
    for(BitMap *b = first; b; b = b->next){
prt(cerr, b->go, b->on); cerr << endl;
	 ++n;
    }
    cerr << n << " bitmaps\n";
    first = NULL;
}

void genGoTo(FILE *o, State *to){
    fprintf(o, "\tgoto yy%u;\n", to->label);
}

void genIf(FILE *o, const char *cmp, uint v){
    fprintf(o, "\tif(yych %s '", cmp);
    prtCh(o, v);
    fputs("')", o);
}

void indent(ostream &o, uint i){
    while(i-- > 0)
	o << "\t";
}

static void need(ostream &o, uint n){
    if(n == 1)
	o << "\tif(YYLIMIT == YYCURSOR) YYFILL(1);\n";
    else
	o << "\tif((YYLIMIT - YYCURSOR) < " << n << ") YYFILL(" << n << ");\n";
    o << "\tyych = *YYCURSOR;\n";
}

void
Action_emit(Action *a, FILE *o)
{
    int first = 1;
    uint i;
    uint back;

    switch (a->type) {
	case MATCHACT:
	    if(a->state->link){
		fputs("\t++YYCURSOR;\n", o);
		need(o, a->state->depth);
	    } else {
		fputs("\tyych = *++YYCURSOR;\n", o);
	    }
	    break;
	case ENTERACT:
	    if(a->state->link){
		fputs("\t++YYCURSOR;\n", o);
		fprintf(o, "yy%u:\n", a->d.label);
		need(o, a->state->depth);
	    } else {
		fputs("\tyych = *++YYCURSOR;\n", o);
		fprintf(o, "yy%u:\n", a->d.label);
	    }
	    break;
	case SAVEMATCHACT:
	    fprintf(o, "\tyyaccept = %u;\n", selector);
	    if(a->state->link){
		fputs("\tYYMARKER = ++YYCURSOR;\n", o);
		need(o, a->state->depth);
	    } else {
		fputs("\tyych = *(YYMARKER = ++YYCURSOR);\n", o);
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
			fputs("\tswitch(yyaccept){\n", o);
		    }
		    fprintf(o, "\tcase %u:", a->d.Accept.saves[i]);
		    genGoTo(o, a->d.Accept.rules[i]);
		}
	    if(!first)
		fputs("\t}\n", o);
	    break;
	case RULEACT:
	    back = RegExp_fixedLength(a->d.rule->d.RuleOp.ctx);
	    if(back != ~0u && back > 0u)
		fprintf(o, "\tYYCURSOR -= %u;", back);
	    fprintf(o, "\n#line %u\n\t", a->d.rule->d.RuleOp.code->line);
	    Str_out(o, a->d.rule->d.RuleOp.code->text);
	    fprintf(o, "\n");
	    break;
    }
}

Action *
Action_new_Accept(State *x, uint n, uint *s, State **r)
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

void doLinear(FILE *o, uint i, Span *s, uint n, State *next){
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

void genCases(ostream &o, uint lb, Span *s){
    if(lb < s->ub){
	for(;;){
	    o << "\tcase '"; prtCh(o, lb); o << "':";
	    if(++lb == s->ub)
		break;
	    o << "\n";
	}
    }
}

void
Go_genSwitch(Go *g, FILE *o, State *next){
    if(g->nSpans <= 2){
	genLinear(o, next);
    } else {
	State *def = g->span[g->nSpans-1].to;
	Span **sP = malloc(sizeof(Span*)*(g->nSpans-1)), **r, **s, **t;
	uint i;

	t = &sP[0];
	for(i = 0; i < g->nSpans; ++i)
	    if(g->span[i].to != def)
		*(t++) = &g->span[i];

	fputs("\tswitch(yych){\n", o);
	while(t != &sP[0]){
	    State *to;
	    r = s = &sP[0];
	    if(*s == &g->span[0])
		genCases(o, 0, *s);
	    else
		genCases(o, (*s)[-1].ub, *s);
	    to = (*s)->to;
	    while(++s < t){
		if((*s)->to == g->to)
		    genCases(o, (*s)[-1].ub, *s);
		else
		    *(r++) = *s;
	    }
	    genGoTo(o, g->to);
	    t = r;
	}
	fputs("\tdefault:", o);
	genGoTo(o, def);
	fputs("\t}\n", o);

	free(sP);
    }
}

void doBinary(FILE *o, uint i, Span *s, uint n, State *next){
    if(n <= 4){
	doLinear(o, i, s, n, next);
    } else {
	uint h = n/2;
	indent(o, i); genIf(o, "<=", s[h-1].ub - 1); fputs("{\n", o);
	doBinary(o, i+1, &s[0], h, next);
	indent(o, i); fputs("\t} else {\n", o);
	doBinary(o, i+1, &s[h], n - h, next);
	indent(o, i); fputs("\t}\n", o);
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
	uint util;
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
    if(nSpans > 5){
	Go_genBinary(g, o, next);
    } else {
	Go_genLinear(g, o, next);
    }
}

void
Go_genGoto(Go *g, FILE *o, State *next){
    uint i;
    if(bFlag){
	for(i = 0; i < g->nSpans; ++i){
	    State *to = g->span[i].to;
	    if(to && to->isBase){
		BitMap *b = BitMap::find(to);
		if(b && matches(b->go, b->on, g, to)){
		    Go go;
		    go.span = malloc(sizeof(Span)*g->nSpans);
		    go.unmap(g, to);
		    o << "\tif(yybm[" << b->i << "+yych] & " << (uint) b->m << ")";
		    genGoTo(o, to);
		    Go_genBase(go, o, next);
		    free(go.span);
		    return;
		}
	    }
	}
    }
    Go_genBase(g, o, next);
}

void State::emit(ostream &o){
    o << "yy" << label << ":";
    action->emit(o);
}

uint merge(Span *x0, State *fg, State *bg){
    Span *x = x0, *f = fg->go.span, *b = bg->go.span;
    uint nf = fg->go.nSpans, nb = bg->go.nSpans;
    State *prev = NULL, *to;
    // NB: we assume both spans are for same range
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

const uint cInfinity = ~0;

class SCC {
public:
    State	**top, **stk;
public:
    SCC(uint);
    ~SCC();
    void traverse(State*);
};

SCC::SCC(uint size){
    top = stk = new State*[size];
}

SCC::~SCC(){
    delete [] stk;
}

void SCC::traverse(State *x){
    *top = x;
    uint k = ++top - stk;
    x->depth = k;
    for(uint i = 0; i < x->go.nSpans; ++i){
	State *y = x->go.span[i].to;
	if(y){
	    if(y->depth == 0)
		traverse(y);
	    if(y->depth < x->depth)
		x->depth = y->depth;
	}
    }
    if(x->depth == k)
	do {
	    (*--top)->depth = cInfinity;
	    (*top)->link = x;
	} while(*top != x);
}

uint maxDist(State *s){
    uint mm = 0;
    for(uint i = 0; i < s->go.nSpans; ++i){
	State *t = s->go.span[i].to;
	if(t){
	    uint m = 1;
	    if(!t->link)
		m += maxDist(t);
	    if(m > mm)
		mm = m;
	}
    }
    return mm;
}

void calcDepth(State *head){
    State *t;
    for(State *s = head; s; s = s->next){
	if(s->link == s){
	    for(uint i = 0; i < s->go.nSpans; ++i){
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
 
void DFA::findSCCs(){
    SCC scc(nStates);
    State *s;

    for(s = head; s; s = s->next){
	s->depth = 0;
	s->link = NULL;
    }

    for(s = head; s; s = s->next)
	if(!s->depth)
	    scc.traverse(s);

    calcDepth(head);
}

void DFA::split(State *s){
    State *move = new State;
    (void) new Move(move);
    addState(&s->next, move);
    move->link = s->link;
    move->rule = s->rule;
    move->go = s->go;
    s->rule = NULL;
    s->go.nSpans = 1;
    s->go.span = new Span[1];
    s->go.span[0].ub = ubChar;
    s->go.span[0].to = move;
}

void DFA::emit(ostream &o){
    static uint label = 0;
    State *s;
    uint i;

    findSCCs();
    head->link = head;
    head->depth = maxDist(head);

    uint nRules = 0;
    for(s = head; s; s = s->next)
	if(s->rule && s->rule->accept >= nRules)
		nRules = s->rule->accept + 1;

    uint nSaves = 0;
    uint *saves = new uint[nRules];
    memset(saves, ~0, (nRules)*sizeof(*saves));

    // mark backtracking points
    for(s = head; s; s = s->next){
	RuleOp *ignore = NULL;
	if(s->rule){
	    for(i = 0; i < s->go.nSpans; ++i)
		if(s->go.span[i].to && !s->go.span[i].to->rule){
		    delete s->action;
		    if(saves[s->rule->accept] == ~0u)
			saves[s->rule->accept] = nSaves++;
		    (void) new Save(s, saves[s->rule->accept]);
		    continue;
		}
	    ignore = s->rule;
	}
    }

    // insert actions
    State **rules = new State*[nRules];
    memset(rules, 0, (nRules)*sizeof(*rules));
    State *accept = NULL;
    for(s = head; s; s = s->next){
	State *ow;
	if(!s->rule){
	    ow = accept;
	} else {
	    if(!rules[s->rule->accept]){
		State *n = new State;
		(void) new Rule(n, s->rule);
		rules[s->rule->accept] = n;
		addState(&s->next, n);
	    }
	    ow = rules[s->rule->accept];
	}
	for(i = 0; i < s->go.nSpans; ++i)
	    if(!s->go.span[i].to){
		if(!ow){
		    ow = accept = new State;
		    (void) new Accept(accept, nRules, saves, rules);
		    addState(&s->next, accept);
		}
		s->go.span[i].to = ow;
	    }
    }

    // split ``base'' states into two parts
    for(s = head; s; s = s->next){
	s->isBase = false;
	if(s->link){
	    for(i = 0; i < s->go.nSpans; ++i){
		if(s->go.span[i].to == s){
		    s->isBase = true;
		    split(s);
		    if(bFlag)
			BitMap::find(&s->next->go, s);
		    s = s->next;
		    break;
		}
	    }
	}
    }

    // find ``base'' state, if possible
    Span *span = new Span[ubChar - lbChar];
    for(s = head; s; s = s->next){
	if(!s->link){
	    for(i = 0; i < s->go.nSpans; ++i){
		State *to = s->go.span[i].to;
		if(to && to->isBase){
		    to = to->go.span[0].to;
		    uint nSpans = merge(span, s, to);
		    if(nSpans < s->go.nSpans){
			delete [] s->go.span;
			s->go.nSpans = nSpans;
			s->go.span = new Span[nSpans];
			memcpy(s->go.span, span, nSpans*sizeof(Span));
		    }
		    break;
		}
	    }
	}
    }
    delete [] span;

    delete head->action;

    o << "{\n\tYYCTYPE yych;\n\tunsigned int yyaccept;\n";

    if(bFlag)
	BitMap::gen(o, lbChar, ubChar);

    o << "\tgoto yy" << label << ";\n";
    (void) new Enter(head, label++);

    for(s = head; s; s = s->next)
	s->label = label++;

    for(s = head; s; s = s->next){
	s->emit(o);
	s->go.genGoto(o, s->next);
    }
    o << "}\n";

    BitMap::first = NULL;

    delete [] saves;
    delete [] rules;
}
