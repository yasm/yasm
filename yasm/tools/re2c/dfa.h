#ifndef re2c_dfa_h
#define re2c_dfa_h

#include <stdio.h>
#include "tools/re2c/re.h"

extern void prtCh(FILE *, uchar);
extern void printSpan(FILE *, uint, uint);

struct DFA;
struct State;

typedef enum {
    MATCHACT = 1,
    ENTERACT,
    SAVEMATCHACT,
    MOVEACT,
    ACCEPTACT,
    RULEACT
} ActionType;

typedef struct Action {
    struct State	*state;
    ActionType		type;
    union {
	/* data for Enter */
	uint		label;
	/* data for SaveMatch */
	uint		selector;
	/* data for Accept */
	struct {
	    uint		nRules;
	    uint		*saves;
	    struct State	**rules;
	} Accept;
	/* data for Rule */
	RegExp		*rule;	/* RuleOp */
    } d;
} Action;

void Action_emit(Action*, FILE *);

typedef struct Span {
    uint		ub;
    struct State	*to;
} Span;

uint Span_show(Span*, FILE *, uint);

typedef struct Go {
    uint		nSpans;
    Span		*span;
} Go;

typedef struct State {
    uint		label;
    RegExp		*rule;	/* RuleOp */
    struct State	*next;
    struct State	*link;
    uint		depth;		/* for finding SCCs */
    uint		kCount;
    Ins			**kernel;
    uint		isBase:1;
    Go			go;
    Action		*action;
} State;

void Go_genGoto(Go*, FILE *, State*);
void Go_genBase(Go*, FILE *, State*);
void Go_genLinear(Go*, FILE *, State*);
void Go_genBinary(Go*, FILE *, State*);
void Go_genSwitch(Go*, FILE *, State*);
void Go_compact(Go*);
void Go_unmap(Go*, Go*, State*);

State *State_new(void);
void State_delete(State*);
void State_emit(State*, FILE *);
void State_out(FILE *, const State*);

typedef struct DFA {
    uint		lbChar;
    uint		ubChar;
    uint		nStates;
    State		*head, **tail;
    State		*toDo;
} DFA;

DFA *DFA_new(Ins*, uint, uint, uint, Char*);
void DFA_delete(DFA*);
void DFA_addState(DFA*, State**, State*);
State *DFA_findState(DFA*, Ins**, uint);
void DFA_split(DFA*, State*);

void DFA_findSCCs(DFA*);
void DFA_emit(DFA*, FILE *);
void DFA_out(FILE *, const DFA*);

static inline Action *
Action_new_Match(State *s)
{
    Action *a = malloc(sizeof(Action));
    a->type = MATCHACT;
    a->state = s;
    s->action = a;
    return a;
}

static inline Action *
Action_new_Enter(State *s, uint l)
{
    Action *a = malloc(sizeof(Action));
    a->type = ENTERACT;
    a->state = s;
    a->d.label = l;
    s->action = a;
    return a;
}

static inline Action *
Action_new_Save(State *s, uint i)
{
    Action *a = malloc(sizeof(Action));
    a->type = SAVEMATCHACT;
    a->state = s;
    a->d.selector = i;
    s->action = a;
    return a;
}

static inline Action *
Action_new_Move(State *s)
{
    Action *a = malloc(sizeof(Action));
    a->type = MOVEACT;
    a->state = s;
    s->action = a;
    return a;
}

Action *Action_new_Accept(State*, uint, uint*, State**);

static inline Action *
Action_new_Rule(State *s, RegExp *r) /* RuleOp */
{
    Action *a = malloc(sizeof(Action));
    a->type = RULEACT;
    a->state = s;
    a->d.rule = r;
    s->action = a;
    return a;
}

#endif
