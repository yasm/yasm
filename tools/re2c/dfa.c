#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "globals.h"
#include "substr.h"
#include "dfa.h"

#define octCh(c) ('0' + c%8)

void prtCh(FILE *o, uchar c){
    uchar oc = talx[c];
    switch(oc){
    case '\'': fputs("\\'", o); break;
    case '\n': fputs("\\n", o); break;
    case '\t': fputs("\\t", o); break;
    case '\v': fputs("\\v", o); break;
    case '\b': fputs("\\b", o); break;
    case '\r': fputs("\\r", o); break;
    case '\f': fputs("\\f", o); break;
    case '\a': fputs("\\a", o); break;
    case '\\': fputs("\\\\", o); break;
    default:
	if(isprint(oc))
	    fputc(oc, o);
	else
	    fprintf(o, "\\%c%c%c", octCh(c/64), octCh(c/8), octCh(c));
    }
}

void printSpan(FILE *o, uint lb, uint ub){
    if(lb > ub)
	fputc('*', o);
    fputc('[', o);
    if((ub - lb) == 1){
	prtCh(o, lb);
    } else {
	prtCh(o, lb);
	fputc('-', o);
	prtCh(o, ub-1);
    }
    fputc(']', o);
}

uint
Span_show(Span *s, FILE *o, uint lb)
{
    if(s->to){
	printSpan(o, lb, s->ub);
	fprintf(o, " %u; ", s->to->label);
    }
    return ub;
}

void
State_out(FILE *o, const State *s){
    uint lb, i;
    fprintf(o, "state %u", s->label);
    if(s->rule)
	fprintf(o, " accepts %u", s->rule->d.RuleOp.accept);
    fputs("\n", o);
    lb = 0;
    for(i = 0; i < s->go.nSpans; ++i)
	lb = s->go.span[i].show(o, lb);
}

void
DFA_out(FILE *o, const DFA *dfa){
    State *s;
    for(s = dfa->head; s; s = s->next) {
	State_out(o, s);
	fputs("\n\n", o);
    }
}

State *
State_new(void)
{
    State *s = malloc(sizeof(State));
    s->rule = s->link = NULL;
    s->kCount = 0;
    s->kernel = s->action = NULL;
    s->go.nSpans = 0;
    s->go.span = NULL;
    return s;
}

void
State_delete(State *s)
{
    free(s->kernel);
    free(s->go.span);
    free(s);
}

static Ins **closure(Ins **cP, Ins *i){
    while(!isMarked(i)){
	mark(i);
	*(cP++) = i;
	if(i->i.tag == FORK){
	    cP = closure(cP, i + 1);
	    i = (Ins*) i->i.link;
	} else if(i->i.tag == GOTO){
	    i = (Ins*) i->i.link;
	} else
	    break;
    }
    return cP;
}

struct GoTo {
    Char	ch;
    void	*to;
};

DFA::DFA(Ins *ins, uint ni, uint lb, uint ub, Char *rep)
    : lbChar(lb), ubChar(ub) {
    Ins **work = new Ins*[ni+1];
    uint nc = ub - lb;
    GoTo *goTo = new GoTo[nc];
    Span *span = new Span[nc];
    memset((char*) goTo, 0, nc*sizeof(GoTo));
    tail = &head;
    head = NULL;
    nStates = 0;
    toDo = NULL;
    findState(work, closure(work, &ins[0]) - work);
    while(toDo){
	State *s = toDo;
	toDo = s->link;

	Ins **cP, **iP, *i;
	uint nGoTos = 0;
	uint j;

	s->rule = NULL;
	for(iP = s->kernel; (i = *iP); ++iP){
	    if(i->i.tag == CHAR){
		for(Ins *j = i + 1; j < (Ins*) i->i.link; ++j){
		    if(!(j->c.link = goTo[j->c.value - lb].to))
			goTo[nGoTos++].ch = j->c.value;
		    goTo[j->c.value - lb].to = j;
		}
	    } else if(i->i.tag == TERM){
		if(!s->rule || ((RuleOp*) i->i.link)->accept < s->rule->accept)
		    s->rule = (RuleOp*) i->i.link;
	    }
	}

	for(j = 0; j < nGoTos; ++j){
	    GoTo *go = &goTo[goTo[j].ch - lb];
	    i = (Ins*) go->to;
	    for(cP = work; i; i = (Ins*) i->c.link)
		cP = closure(cP, i + i->c.bump);
	    go->to = findState(work, cP - work);
	}

	s->go.nSpans = 0;
	for(j = 0; j < nc;){
	    State *to = (State*) goTo[rep[j]].to;
	    while(++j < nc && goTo[rep[j]].to == to);
	    span[s->go.nSpans].ub = lb + j;
	    span[s->go.nSpans].to = to;
	    s->go.nSpans++;
	}

	for(j = nGoTos; j-- > 0;)
	    goTo[goTo[j].ch - lb].to = NULL;

	s->go.span = new Span[s->go.nSpans];
	memcpy((char*) s->go.span, (char*) span, s->go.nSpans*sizeof(Span));

	(void) new Match(s);

    }
    delete [] work;
    delete [] goTo;
    delete [] span;
}

DFA::~DFA(){
    State *s;
    while((s = head)){
	head = s->next;
	delete s;
    }
}

void DFA::addState(State **a, State *s){
    s->label = nStates++;
    s->next = *a;
    *a = s;
    if(a == tail)
	tail = &s->next;
}

State *DFA::findState(Ins **kernel, uint kCount){
    Ins **cP, **iP, *i;
    State *s;

    kernel[kCount] = NULL;

    cP = kernel;
    for(iP = kernel; (i = *iP); ++iP){
	 if(i->i.tag == CHAR || i->i.tag == TERM){
	     *cP++ = i;
	} else {
	     unmark(i);
	}
    }
    kCount = cP - kernel;
    kernel[kCount] = NULL;

    for(s = head; s; s = s->next){
	 if(s->kCount == kCount){
	     for(iP = s->kernel; (i = *iP); ++iP)
		 if(!isMarked(i))
		     goto nextState;
	     goto unmarkAll;
	 }
	 nextState:;
    }

    s = new State;
    addState(tail, s);
    s->kCount = kCount;
    s->kernel = new Ins*[kCount+1];
    memcpy(s->kernel, kernel, (kCount+1)*sizeof(Ins*));
    s->link = toDo;
    toDo = s;

unmarkAll:
    for(iP = kernel; (i = *iP); ++iP)
	 unmark(i);

    return s;
}
