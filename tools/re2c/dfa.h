#ifndef _dfa_h
#define _dfa_h

#include <iostream.h>
#include "re.h"

extern void prtCh(ostream&, uchar);
extern void printSpan(ostream&, uint, uint);

class DFA;
class State;

class Action {
public:
    State		*state;
public:
    Action(State*);
    virtual void emit(ostream&) = 0;
};

class Match: public Action {
public:
    Match(State*);
    void emit(ostream&);
};

class Enter: public Action {
public:
    uint		label;
public:
    Enter(State*, uint);
    void emit(ostream&);
};

class Save: public Match {
public:
    uint		selector;
public:
    Save(State*, uint);
    void emit(ostream&);
};

class Move: public Action {
public:
    Move(State*);
    void emit(ostream&);
};

class Accept: public Action {
public:
    uint		nRules;
    uint		*saves;
    State		**rules;
public:
    Accept(State*, uint, uint*, State**);
    void emit(ostream&);
};

class Rule: public Action {
public:
    RuleOp		*rule;
public:
    Rule(State*, RuleOp*);
    void emit(ostream&);
};

class Span {
public:
    uint		ub;
    State		*to;
public:
    uint show(ostream&, uint);
};

class Go {
public:
    uint		nSpans;
    Span		*span;
public:
    void genGoto(ostream&, State*);
    void genBase(ostream&, State*);
    void genLinear(ostream&, State*);
    void genBinary(ostream&, State*);
    void genSwitch(ostream&, State*);
    void compact();
    void unmap(Go*, State*);
};

class State {
public:
    uint		label;
    RuleOp		*rule;
    State		*next;
    State		*link;
    uint		depth;		// for finding SCCs
    uint		kCount;
    Ins			**kernel;
    bool		isBase:1;
    Go			go;
    Action		*action;
public:
    State();
    ~State();
    void emit(ostream&);
    friend ostream& operator<<(ostream&, const State&);
    friend ostream& operator<<(ostream&, const State*);
};

class DFA {
public:
    uint		lbChar;
    uint		ubChar;
    uint		nStates;
    State		*head, **tail;
    State		*toDo;
public:
    DFA(Ins*, uint, uint, uint, Char*);
    ~DFA();
    void addState(State**, State*);
    State *findState(Ins**, uint);
    void split(State*);

    void findSCCs();
    void emit(ostream&);

    friend ostream& operator<<(ostream&, const DFA&);
    friend ostream& operator<<(ostream&, const DFA*);
};

inline Action::Action(State *s) : state(s) {
    s->action = this;
}

inline Match::Match(State *s) : Action(s)
    { }

inline Enter::Enter(State *s, uint l) : Action(s), label(l)
    { }

inline Save::Save(State *s, uint i) : Match(s), selector(i)
    { }

inline ostream& operator<<(ostream &o, const State *s)
    { return o << *s; }

inline ostream& operator<<(ostream &o, const DFA *dfa)
    { return o << *dfa; }

#endif
