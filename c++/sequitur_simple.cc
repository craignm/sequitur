#include <iostream>
using namespace std;

// This code was written for didactic purposes;
// it is feature-poor in order to be as simple as possible.
// For a more realistic implementation of Sequitur, please use
// http://sequitur.info/latest/sequitur.tgz

class symbols;
class rules;

// This finds a matching digram in the grammar, and returns a pointer
// to its entry in the hash table so it can be changed or deleted as necessary

extern symbols **find_digram(symbols *s);

///////////////////////////////////////////////////////////////////////////

class rules {
  // the guard node in the linked list of symbols that make up the rule
  // It points forward to the first symbol in the rule, and backwards
  // to the last symbol in the rule. Its own value points to the rule data
  // structure, so that symbols can find out which rule they're in

  symbols *guard;

  //  count keeps track of the number of times the rule is used in the grammar
  int count;

  // this is just for numbering the rules nicely for printing; it's
  // not essential for the algorithm
  int number;

public:
  rules();
  ~rules();

  void reuse() { count ++; }
  void deuse() { count --; };

  symbols *first();
  symbols *last();

  int freq() { return count; };
  int index() { return number; };
  void index(int i) { number = i; };
};

class symbols {
  symbols *n, *p;
  ulong s;

 public:

  // initializes a new symbol. If it is non-terminal, increments the reference
  // count of the corresponding rule

  symbols(ulong sym) {
    s = sym * 2 + 1;
    // an odd number, so that they're distinct from the rule pointers, which
    // we assume are 4-byte aligned
    p = n = 0;
  }

  symbols(rules *r) {
    s = (ulong) r;
    p = n = 0;
    rule()->reuse();
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
        *find_digram(right) = right;
      }

      if (left->p && left->n &&
          left->value() == left->n->value() &&
          left->value() == left->p->value()) {
        *find_digram(left->p) = left->p;
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
  }

  // inserts a symbol after this one.

  void insert_after(symbols *y) {
    join(y, n);
    join(this, y);
  };

  // removes the digram from the hash table

  void delete_digram() {
    if (is_guard() || n->is_guard()) return;
    symbols **m = find_digram(this);
    if (*m == this) *m = (symbols *) 1;
  }

  // returns true if this is the guard node marking the beginning/end of a rule

  int is_guard() { return non_terminal() && rule()->first()->prev() == this; };

  // non_terminal() returns true if a symbol is non-terminal.
  // If s is odd, the symbol is a terminal
  // If s is even, the symbol is  non-terminal: a pointer to the referenced rule

  int non_terminal() { return ((s % 2) == 0) && (s != 0);};

  symbols *next() { return n;};
  symbols *prev() { return p;};
  inline ulong raw_value() {  return s; };
  inline ulong value() { return s / 2;};

  // assuming this is a non-terminal, rule() returns the corresponding rule

  rules *rule() { return (rules *) s;};

  void substitute(rules *r);
  static void match(symbols *s, symbols *m);

  // checks a new digram. If it appears elsewhere, deals with it by calling
  // match(), otherwise inserts it into the hash table

  int check() {
    if (is_guard() || n->is_guard()) return 0;
    symbols **x = find_digram(this);
    if (ulong(*x) <= 1) {
      *x = this;
      return 0;
    }

    if (ulong(*x) > 1 && (*x)->next() != this) match(this, *x);
    return 1;
  }

  void expand();

  void point_to_self() { join(this, this); };
};

rules S;

main()
{
  S.last()->insert_after(new symbols(cin.get()));
  int x = 0;

  while (1) {
    int i = cin.get(); if (cin.eof()) break;
    S.last()->insert_after(new symbols(i));
    S.last()->prev()->check();
  }

  void print();
  print();
}

int num_rules = 0;

rules::rules()
{
  num_rules ++;
  guard = new symbols(this);
  guard->point_to_self();
  count = number = 0;
}

rules::~rules() {
  num_rules --;
  delete guard;
}

symbols *rules::first() { return guard->next(); }
symbols *rules::last() { return guard->prev(); }

// This symbol is the last reference to its rule. It is deleted, and the
// contents of the rule substituted in its place.

void symbols::expand() {
  symbols *left = prev();
  symbols *right = next();
  symbols *f = rule()->first();
  symbols *l = rule()->last();

  delete rule();
  symbols **m = find_digram(this);
  if (*m == this) *m = (symbols *) 1;
  s = 0; // if we don't do this, deleting the symbol tries to deuse the rule!
  delete this;

  join(left, f);
  join(l, right);

  *find_digram(l) = l;
}

// Replace a digram with a non-terminal

void symbols::substitute(rules *r)
{
  symbols *q = p;

  delete q->next();
  delete q->next();

  q->insert_after(new symbols(r));

  if (!q->check()) q->n->check();
}

// Deal with a matching digram

void symbols::match(symbols *ss, symbols *m)
{
  rules *r;

  // reuse an existing rule
  if (m->prev()->is_guard() && m->next()->next()->is_guard()) {
    r = m->prev()->rule();
    ss->substitute(r);
  }
  else {
    // create a new rule
    r = new rules;

    if (ss->non_terminal())
      r->last()->insert_after(new symbols(ss->rule()));
    else
      r->last()->insert_after(new symbols(ss->value()));

    if (ss->next()->non_terminal())
      r->last()->insert_after(new symbols(ss->next()->rule()));
    else
      r->last()->insert_after(new symbols(ss->next()->value()));

    m->substitute(r);
    ss->substitute(r);

    *find_digram(r->first()) = r->first();
  }

  // check for an underused rule

  if (r->first()->non_terminal() && r->first()->rule()->freq() == 1) r->first()->expand();
}

// pick a prime! (large enough to hold every digram in the grammar, with room
// to spare

#define PRIME 2265539

// Standard open addressing or double hashing. See Knuth.

#define HASH(one, two) (((one) << 16 | (two)) % PRIME)
#define HASH2(one) (17 - ((one) % 17))

symbols *table[PRIME];

symbols **find_digram(symbols *s)
{
  ulong one = s->raw_value();
  ulong two = s->next()->raw_value();

  int jump = HASH2(one);
  int insert = -1;
  int i = HASH(one, two);

  while (1) {
    symbols *m = table[i];
    if (!m) {
      if (insert == -1) insert = i;
      return &table[insert];
    }
    else if (ulong(m) == 1) insert = i;
    else if (m->raw_value() == one && m->next()->raw_value() == two)
      return &table[i];
    i = (i + jump) % PRIME;
  }
}

// print the rules out

rules **R;
int Ri;

void p(rules *r) {
  for (symbols *p = r->first(); !p->is_guard(); p = p->next())
    if (p->non_terminal()) {
      int i;

      if (R[p->rule()->index()] == p->rule())
        i = p->rule()->index();
      else {
        i = Ri;
        p->rule()->index(Ri);
        R[Ri ++] = p->rule();
      }

      cout << i << ' ';
    }
    else {
      if (p->value() == ' ') cout << '_';
      else if (p->value() == '\n') cout << "\\n";
      else if (p->value() == '\t') cout << "\\t";
      else if (p->value() == '\\' ||
               p->value() == '(' ||
               p->value() == ')' ||
               p->value() == '_' ||
               isdigit(p->value()))
        cout << '\\' << char(p->value());
      else cout << char(p->value());
      cout << ' ';
    }
  cout << endl;
}

void print()
{
  R = (rules **) malloc(sizeof(rules *) * num_rules);
  memset(R, 0, sizeof(rules *) * num_rules);
  R[0] = &S;
  Ri = 1;

  for (int i = 0; i < Ri; i ++) {
    cout << i << " -> ";
    p(R[i]);
  }
  free(R);
}
