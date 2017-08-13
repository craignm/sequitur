/****************************************************************************

 classes.h - Contains definitions of 'rule' and 'symbols' classes, and the
             bodies of some of their methods.

****************************************************************************/

#include <assert.h>
#include <fstream>
#include <iostream>
#include <memory.h> // for memset
#include <stdlib.h> // for malloc

using namespace std;

extern int num_symbols, current_rule, K;
extern int occupied, table_size;

class symbols;
class rules;
ostream &operator << (ostream &o, symbols &s);

typedef unsigned long ulong;

extern symbols **find_digram(symbols *s);     // defined in classes.cc

///////////////////////////////////////////////////////////////////////////

class rules {

  // the guard node in the linked list of symbols that make up the rule
  // It points forward to the first symbol in the rule, and backwards
  // to the last symbol in the rule. Its own value points to the rule data
  // structure, so that symbols can find out which rule they're in
  symbols *guard;

  // count keeps track of the number of times the rule is used in the grammar
  int count;

  // Usage stores the number of times a rule is used in the input.
  //    An example of the difference between count and Usage: in a grammar
  //    with rules S->abXcdXef , X->gAiAj , A->kl , rule A's count is 2
  //    (it is used two times in the grammar), while its Usage is 4 (there
  //    are two X's in the input sequence, and each of them uses A two times)
  int Usage;

  // number can serve two purposes:
  // (1) numbering the rules nicely for printing
  //     (in this case it's not essential for the algorithm)
  // (2) if this is a non-terminal symbol, assign a code to it
  //     (used in compression and forget_print())
  int number;

public:
  void output();     // output right hand of the rule when printing out grammar
  void output2();    // output right hand of the rule when compressing

  rules();
  ~rules();

  void reuse() { count ++; }
  void deuse() { count --; }

  symbols *first();     // pointer to first symbol of rule's right hand
  symbols *last();      // pointer to last symbol of rule's right hand

  int freq()           { return count; }
  int usage()          { return Usage; }
  void usage(int i)    { Usage += i; }
  int index()          { return number; }
  void index(int i)    { number = i; }

  void reproduce();    // reproduce full expansion of the rule
};

class symbols {
  symbols *n, *p;     // next and previous symbol within the rule
  ulong s;            // symbol value (e.g. ASCII code, or rule index)

public:

  // print out symbol, or, if it is non-terminal, rule's full expansion
  void reproduce() {
    extern int numbers;

    if (non_terminal()) rule()->reproduce();
    else {
      cout << *this;
      if (numbers) cout << ' ';
    }
  }

  // initializes a new terminal symbol
  symbols(ulong sym) {
    s = sym * 2 + 1; // an odd number, so that they're a distinct
                     // space from the rule pointers, which are 4-byte aligned
    p = n = 0;
    num_symbols ++;
  }

  // initializes a new symbol to refer to a rule, and increments the reference
  // count of the corresponding rule
  symbols(rules *r) {
    s = (ulong) r;
    p = n = 0;
    rule()->reuse();
    num_symbols ++;
  }

  // links two symbols together, removing any old digram from the hash table
  static void join(symbols *left, symbols *right) {
    if (left->n) {
      left->delete_digram();

      // This is to deal with triples, where we only record the second
      // pair of the overlapping digrams. When we delete the second pair,
      // we insert the first pair into the hash table so that we don't
      // forget about it.  e.g. abbbabcbb

      if (right->p && right->n &&
          right->value() == right->p->value() &&
          right->value() == right->n->value()) {
        assert(find_digram(right));
        symbols **r = find_digram(right);
        if (r) { // necessary when using delimiters
          *r = right;
          occupied ++;
        }
      }

      if (left->p && left->n &&
          left->value() == left->n->value() &&
          left->value() == left->p->value()) {
        symbols **lp = find_digram(left->p);
        if (lp) { // necessary when using delimiters
          *lp = left->p;
          occupied ++;
        }
      }
    }
    left->n = right; right->p = left;
  }

  // cleans up for symbol deletion: removes hash table entry and decrements
  // rule reference count
  ~symbols() {
    join(p, n);
    if (!is_guard()) {
      delete_digram();
      if (non_terminal()) rule()->deuse();
    }
    num_symbols --;
  }

  // inserts a symbol after this one.
  void insert_after(symbols *y) {
    join(y, n);
    join(this, y);
  }

  // removes the digram from the hash table
  void delete_digram() {
    if (is_guard() || n->is_guard()) return;
    symbols **m = find_digram(this);
    if (m == 0) return;
    for (int i = 0; i < K; i ++)
      if (m[i] == this) {
	m[i] = (symbols *) 1;
	occupied --;
      }
  }

  // is_guard() returns true if this is the guard node
  // marking the beginning/end of a rule
  bool is_guard() { return non_terminal() && rule()->first()->prev() == this; }

  // non_terminal() returns true if a symbol is non-terminal.
  // We make sure that terminals have odd-numbered values.
  // Because the value of a non-terminal is a pointer to
  // the corresponding rule, they're guaranteed to be even
  // (more -- a multiple of 4) on modern architectures

  int non_terminal() { return ((s % 2) == 0) && (s != 0);}

  symbols *next() { return n; }
  symbols *prev() { return p; }
  inline ulong raw_value() { return s; }
  inline ulong value() { return s / 2; }

  // assuming this is a non-terminal, rule() returns the corresponding rule
  rules *rule() { return (rules *) s; }

  // substitute digram with non-terminal symbol
  void substitute(rules *r);

  // check digram and enforce the Sequitur constraints if necessary
  int check();

  // substitute non-terminal symbol with its rule's right hand
  void expand();

  void point_to_self() { join(this, this); }

};
