/****************************************************************************

 classes.cc - Module containing (part of the) methods of 'rules' and 'symbols'
              classes, and functions for working with the hash table of digrams
              printing out the grammar.

 Notes:
    For the rest of 'symbols' and 'rules' methods, see classes.h .

 ****************************************************************************/

#include "classes.h"
#include <ctype.h>
#include <math.h>

extern int num_rules, delimiter, do_uncompress;

rules::rules() {
  num_rules ++;
  guard = new symbols(this);
  guard->point_to_self();
  count = number = Usage = 0;
}

rules::~rules() {
  num_rules --;
  delete guard;
}

// pointer to first symbol of rule's right hand
symbols *rules::first() { return guard->next(); }
// pointer to  last symbol of rule's right hand
symbols *rules::last()  { return guard->prev(); }

// **************************************************************************
// symbols::check()
//    check digram made of this symbol and the symbol following it,
//    and enforce Sequitur constraints.
//
// Return values
//    0 : did not change the grammar (there was no violation of contraints)
//    1 : did change the grammar (there were violations of contraints)
//
// Global variables used
//    K (minimum number of times a digram must occur to form rule)
// **************************************************************************
int symbols::check() {
  if (is_guard() || n->is_guard()) return 0;

  symbols **x = find_digram(this);
  // if either symbol of the digram is a delimiter -> do nothing
  if (!x) return 0;

  int i;

  // if digram is not yet in the hash table -> put it there, and return
  for (i = 0; i < K; i ++)
    if (ulong(x[i]) <= 1) {
      x[i] = this;
      occupied ++;
      return 0;
    }

  // if repetitions overlap -> do nothing
  for (i = 0; i < K; i ++)
    if (x[i]->next() == this || next() == x[i])
      return 0;

  rules *r;

  // reuse an existing rule

  for (i = 0; i < K; i ++)
    if (x[i]->prev()->is_guard() && x[i]->next()->next()->is_guard()) {
      r = x[i]->prev()->rule();
      substitute(r);

      // check for an underused rule

      if (r->first()->non_terminal() && r->first()->rule()->freq() == 1)
	r->first()->expand();
      // if (r->last()->non_terminal() && r->last()->rule()->freq() == 1)
      //   r->last()->expand();

      return 1;
    }

  symbols *y[100];
  for (i = 0; i < K; i ++) y[i] = x[i];

  // make a copy of the pointers to digrams,
  // so that they don't change under our feet
  // especially when we create this replacement rule

  // create a new rule

  r = new rules;

  if (non_terminal())
    r->last()->insert_after(new symbols(rule()));
  else
    r->last()->insert_after(new symbols(value()));

  if (next()->non_terminal())
    r->last()->insert_after(new symbols(next()->rule()));
  else
    r->last()->insert_after(new symbols(next()->value()));

  for (i = 0; i < K; i ++) {
    if (y[i] == r->first()) continue;
    // check that this hasn't been deleted
    bool deleted = 1;
    for (int j = 0; j < K; j ++)
      if (y[i] == x[j]) {
        deleted = 0;
        break;
      }
    if (deleted) continue;

    y[i]->substitute(r);
    //    y[i] = (symbols *) 1; // should be x
  }

  x[0] = r->first();
  occupied ++;

  substitute(r);

  // check for an underused rule

  if (r->first()->non_terminal() && r->first()->rule()->freq() == 1)
    r->first()->expand();
  //  if (r->last()->non_terminal() && r->last()->rule()->freq() == 1)
  //    r->last()->expand();

  return 1;
}

// ***************************************************************************
// symbols::expand()
//    This symbol is the last reference to its rule. It is deleted, and the
//    contents of the rule substituted in its place.
// ***************************************************************************
void symbols::expand() {
  symbols *left = prev();
  symbols *right = next();
  symbols *f = rule()->first();
  symbols *l = rule()->last();

  extern bool compression_initialized;
  if (!compression_initialized) {
    int i = 0;
    symbols *s;
    extern int max_rule_len;
    // first calculate length of this rule (the one we are adding symbols to)
    // symbol 'this' should not be counted because it will be deleted
    s = next();
    do {
       if (!s->is_guard()) i++;
       s = s->next();
    } while(s != this);
    // then calculate length of what is to be added
    for (s = f; !s->is_guard(); s = s->next()) i++;
    if (i > max_rule_len) max_rule_len = i;
  }

  symbols **m = find_digram(this);
  if (!m) return;
  delete rule();

  for (int i = 0; i < K; i ++)
    if (m[i] == this) {
      m[i] = (symbols *) 1;
      occupied --;
    }

  s = 0; // if we don't do this, deleting the symbol tries to deuse the rule!

  delete this;

  join(left, f);
  join(l, right);

  *find_digram(l) = l;
  occupied ++;
}

// ***************************************************************************
// symbols::substitute(rules *r)
//    Replace digram made up of this symbol and the symbol following it with
//    a non-terminal, which points to rule "r" (parameter).
// ***************************************************************************
void symbols::substitute(rules *r)
{
  symbols *q = p;

  delete q->next();
  delete q->next();

  q->insert_after(new symbols(r));

  if (!q->check()) q->next()->check();
}

int table_size;
int lookups = 0;
int collisions = 0;
int occupied = 0;
symbols **table = 0;

// ***************************************************************************
// symbols **find_digram(symbols *s)
//
//     Search hash table for digram made of symbols s->value() and
//     s->next()->value().
//
// Return value
//   - if digram found : Pointer to hash table element where digram is stored.
//   - otherwise       : 0
//
// Global variables used
//    delimiter  (symbol accross which not to form rules, see sequitur.cc)
// ***************************************************************************
symbols **find_digram(symbols *s)
{
  if (!table) {
    extern int memory_to_use;
    table_size = memory_to_use / (K * sizeof(symbols *));
    extern int quiet;

    if (!quiet) {
      cerr << "Using " << memory_to_use / 1000000
	   << " MB of memory for the hash table." << endl;
      cerr << "If this is too large for your machine, "
	   << "or the hash table becomes more than" << endl;
      cerr << "40% occupied, use -m to specify a new value." << endl << endl;
    }
    // find a prime less than the maximum table size
    if (table_size % 2 == 0)
      table_size --;
    int max_factor = int(sqrt(float(table_size)));
    while (1) {
      int prime = 1;
      for (int i = 3; i < max_factor; i += 2)
	if (table_size % i == 0) {
	  prime = 0;
	  break;
	}
      if (prime)
	break;
      table_size -= 2;
    }

    table = (symbols **) malloc(table_size * K * sizeof(symbols *));
    memset(table, 0, table_size * K * sizeof(symbols *));
  }

  ulong one = s->raw_value();
  ulong two = s->next()->raw_value();

  if (delimiter != -1 && (s->value() == delimiter || s->next()->value() == delimiter))
    return 0;

  int jump = (17 - (one % 17)) * K;
  int insert = -1;

  // Hash function: standard open addressing or double hashing. See Knuth.

  ulong combined = ((one << 16) | (one >> 16)) ^ two;
  int i = ((combined * (combined + 3)) % table_size) * K;
  int original_i = i;

  lookups ++;

  while (1) {
    symbols *m = table[i];
    if (!m) {
      if (insert == -1) insert = i;
      return &table[insert];
    }
    else if (ulong(m) == 1) insert = i;
    else if (m->raw_value() == one && m->next()->raw_value() == two)
      return &table[i];
    i = (i + jump) % table_size;

    // this is only a collision if we're not inserting
    if (insert == -1) 
      collisions ++;
  }
}

// **************************************************************************
// rules::reproduce()
//    Reproduce full expansion of a rule.
// **************************************************************************
void rules::reproduce()
{
  // for each symbol of the rule, call symbols::reproduce()!
  for (symbols *p = first(); !p->is_guard(); p = p->next())
    p->reproduce();
}


// **************************************************************************
// Overload operator << to write symbols of the grammar to streams,
//    in a formatted manner.
// **************************************************************************
ostream &operator << (ostream &o, symbols &s)
{
  extern int numbers;

  if (s.non_terminal())
     o << s.rule()->index();
  else if (numbers & do_uncompress) o << s.value() << endl;
  else if (numbers) o << '[' << s.value() << ']';
  else if (do_uncompress) o << char(s.value());
  else if (s.value() == '\n') o << "\\n";
  else if (s.value() == '\t') o << "\\t";
  else if (s.value() == ' ' ) o << '_';
  else if (s.value() == '\\' ||
       s.value() == '(' ||
       s.value() == ')' ||
       s.value() == '_' ||
       isdigit(s.value()))
    o << '\\' << char(s.value());
  else o << char(s.value());

  return o;
}

// **************************************************************************
// rules::output()

//    Print right hand of this rule. If right hand contains non-terminals,
//    descend recursively and print right hands of subordinated rules, if
//    these have not been printed out yet.
//
//    Also, numbers the rule, which will indicate that right hand has been
//    printed out.
//
// Global variables used
//    current_rule, print_rule_usage
// **************************************************************************
void rules::output()
{
  symbols *s;

  for (s = first(); !s->is_guard(); s = s->next())
     if (s->non_terminal() && s->rule()->index() == 0)
        s->rule()->output();

  number = current_rule ++;

  for (s = first(); !s->is_guard(); s = s->next())
    cout << *s << ' ';

  extern int print_rule_usage;
  if (print_rule_usage) cout << "\t(" << Usage << ")";

  cout << endl;
}
