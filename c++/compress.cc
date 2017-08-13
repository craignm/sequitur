/****************************************************************************

 compress.cc - Module containing functions for compression and decompression
               using Carpinelli, Salamonsen, and Stuiver's revised arithmetic
               coding.

 Description

     Sequitur compression has two stages. The first is inducing a grammar
     from the input, which is implemented in the other modules. Once a grammar
     has been formed, it is sent to the arithmetic coder, which also takes
     care of writing the compressed output. This is implemented here.

     Decompression is a reverse process. Compressed input is read by the
     arithmetic decoder; this gives us a stream of symbols from which we form
     the grammar, and produce back the original sequence along the way.

 Format of the compressed file

     See compressed_file_format.txt .

 ****************************************************************************/


#include <assert.h>
#include <stdio.h>
#include <math.h>

#include "classes.h"

extern "C" {
#include "arith.h"
#include "stats.h"
#include "bitio.h"
}

static context *symbol,            // special symbols, terminals, non-terminals
               *lengths,           // rule lengths
  // codes that indicate whether a rule should be kept in memory or deleted
               *keep;

extern int compress;

// special symbols in the "symbol" context
#define START_RULE       0
#define END_OF_FILE      1
#define STOP_FORGETTING  2
#define SPECIAL_SYMBOLS  3         // how many special symbols there are
#define FIRST_TERMINAL   3         // starting code for terminal symbols
#define FIRST_RULE       4         // starting code for non-terminal symbols

#define MINMAXTERM_TARGET  100000000
#define MAXRULELEN_TARGET  10000

// keep or delete a rule
#define KEEPI_NO       0           // do not keep rule
#define KEEPI_YES      1           // keep rule
#define KEEPI_DUMMY    2           // do not reproduce rule & do not keep it
#define KEEPI_LENGTH   3           // number of symbols in this context


// macros for transforming symbols of the grammar to
// arithmetic-coder codes, and vice versa

#define TERM_TO_CODE(i)      (((i)<<1) + FIRST_TERMINAL)
#define CODE_TO_TERM(i)      (((i) - FIRST_TERMINAL) >> 1)

#define NONTERM_TO_CODE(i)   (((i)<<1) + FIRST_RULE)
#define CODE_TO_NONTERM(i)   (((i) - FIRST_RULE) >> 1)

// whether an arithmetic-coder code represents a terminal or non-terminal
// symbol
#define IS_TERMINAL(code)        ((code) & 1)
#define IS_NONTERMINAL(code)   (!((code) & 1))

// size of array containing pointers to rules, used at decompression
#define UNCOMPRESS_RSIZE  1000000

// --------------------------------------------------------------------------

// Initialize compression or decompression. Create the contexts, start
// writing or reading the compressed file.
//
// Parameter (all_input_read): A boolean value. NOTE: This has a meaning
// only only in the case of compression. For decompression, it makes no
// difference whether you call start_compress(true) or
// start_compress(false).
//
//   false : start_compress() is being called in the middle of reading the
//   input, in order to respect a memory limit. 'symbol' and 'lengths'
//   contexts will be of type DYNAMIC (which allows for escape codes if
//   coding an unknown symbol is attempted - we will need this to code
//   symbols and rule lengths that are yet to appear in the grammar).
//
//   true : start_compress() is being called after all the input has been
//   read.  'symbol' and 'lengths' contexts will be of type STATIC (which
//   compared to DYNAMIC gives somewhat shorter compressed output for the
//   same number of symbols).
//
// --------------------------------------------------------------------------
void start_compress(bool all_input_read)
{
  int i;
  extern int min_terminal, max_terminal, max_rule_len;

  keep = create_context(KEEPI_LENGTH, STATIC);
  install_symbol(keep, KEEPI_NO);
  install_symbol(keep, KEEPI_YES);
  install_symbol(keep, KEEPI_DUMMY);

  binary_context *file_type = create_binary_context();
  int context_type;

  if (compress) {

    // this is specific to compression

    startoutputtingbits();
    start_encode();

    binary_encode(file_type, all_input_read);
    context_type = all_input_read ? STATIC : DYNAMIC;

    min_terminal = TERM_TO_CODE(min_terminal);
    max_terminal = TERM_TO_CODE(max_terminal);

    arithmetic_encode(min_terminal, min_terminal + 1, MINMAXTERM_TARGET);
    arithmetic_encode(max_terminal, max_terminal + 1, MINMAXTERM_TARGET);
    arithmetic_encode(max_rule_len, max_rule_len + 1, MAXRULELEN_TARGET);

  }
  else {

    // this is specific to decompression

    startinputtingbits();
    start_decode();

    context_type = binary_decode(file_type) ? STATIC : DYNAMIC;

    min_terminal = arithmetic_decode_target(MINMAXTERM_TARGET);
    arithmetic_decode(min_terminal, min_terminal + 1, MINMAXTERM_TARGET);
    max_terminal = arithmetic_decode_target(MINMAXTERM_TARGET);
    arithmetic_decode(max_terminal, max_terminal + 1, MINMAXTERM_TARGET);
    max_rule_len = arithmetic_decode_target(MAXRULELEN_TARGET);
    arithmetic_decode(max_rule_len, max_rule_len + 1, MAXRULELEN_TARGET);
  }

  symbol = create_context(SPECIAL_SYMBOLS + max_terminal - min_terminal + 1,
			  context_type);
  install_symbol(symbol, START_RULE);
  install_symbol(symbol, END_OF_FILE);
  install_symbol(symbol, STOP_FORGETTING);
  for (i = min_terminal; i <= max_terminal; i+=2) install_symbol(symbol, i);

  lengths = create_context(max_rule_len, context_type);
  for (i = 2; i <= max_rule_len; i++) install_symbol(lengths, i);

}

static int forgetting = 1;

// Tell the encoder/decoder that no more rules will be deleted from memory.
void stop_forgetting()
{
  encode(symbol, STOP_FORGETTING);
  forgetting = 0;
}

// Finish compression or decompression.
void end_compress() {
  if (compress) {
    encode(symbol, END_OF_FILE);
    finish_encode();
    doneoutputtingbits();
  }
  else {
    finish_decode();
    doneinputtingbits();
  }
}

int current_rule = FIRST_RULE;
static int current_rule_index = 0;

// Encode a rule whose right-hand side has already been encoded.
void encode_rule(rules *r, int keepi)
{
  encode(symbol, r->index());
  if (keepi < KEEPI_LENGTH && forgetting) {
    encode(keep, keepi);
    if (keepi == KEEPI_NO || keepi == KEEPI_DUMMY)
      delete_symbol(symbol, r->index());
  }
}

// Encode a terminal symbol. If it is a symbol not yet known,
// it is added to the context.
void encode_symbol(ulong s)
{
  int i;
  s = TERM_TO_CODE(s);
  if ((i = encode(symbol, s)) == NOT_KNOWN) {
    arithmetic_encode(s, s + 1, MINMAXTERM_TARGET);
    install_symbol(symbol, s);
  }
}

// Sending a rule to the arithmetic coder for the first time --
// encode rule's right-hand side.
// (Here we assign a rule index -- i.e. non-terminal symbol value --
// to the rule and add it to the context).

void rules::output2()
{
  symbols *s;

  number = current_rule;
  current_rule += 2;

  encode(symbol, START_RULE);
  install_symbol(symbol, number);

  int l = 0;
  for (s = first(); !s->is_guard(); s = s->next()) l ++;

  if (encode(lengths, l) == NOT_KNOWN)
     arithmetic_encode(l, l + 1, MAXRULELEN_TARGET);

  for (s = first(); !s->is_guard(); s = s->next())
    if (s->non_terminal() && s->rule()->index() == 0) s->rule()->output2();
    else if (s->non_terminal()) encode_rule(s->rule(), KEEPI_LENGTH);
    else encode_symbol(s->value());
}


/************* Compression entry point *********************/

// Send a symbol of the grammar to the arithmetic coder
// and delete it from the grammar.
void forget(symbols *s)
{
  rules *r = 0;

  // symbol is non-terminal
  if (s->non_terminal()) {
    r = s->rule();
    delete s;

    // this is *not* the last use of this rule in the grammar
    if (r->freq() > 0) {
      // if right-hand side has not been encoded yet, encode it
      if (r->index() == 0) r->output2();
      // else, encode non-terminal symbol (rule is to be kept)
      else encode_rule(r, KEEPI_YES);
    } else {
      // this is the last use of this rule in the grammar
      // right-hand side has not been encoded yet
      if (r->index() == 0) {
	// encode it
        r->output2();
	// signal that rule is to be deleted
        if (forgetting) encode_rule(r, KEEPI_DUMMY);
      }
      // encode non-terminal symbol (rule is to be deleted)
      else encode_rule(r, KEEPI_NO);

      while (r->first()->next() != r->first())               // delete rule
         delete r->first();                                  //
      delete r;                                              //
    }
  }
  else {                                              // symbol is terminal
    encode_symbol(s->value());
    delete s;
  }
}


/************* Decompression functions *********************/

static rules **R;

// Read a symbol from compressed input and return its arithmetic-coder code.
int get_symbol()
{
   int i = decode(symbol);

   if (i == START_RULE) {

   // definition of a new rule

     // current rule's *arithmetic-coder* code
      int n = current_rule;
      current_rule += 2;
      // current rule's *grammar* code
      int ix = current_rule_index++;
      assert(current_rule_index < UNCOMPRESS_RSIZE);

      R[ix] = new rules;
      // add new non-terminal symbol to context
      install_symbol(symbol, n);

      // decode rule length
      int l = decode(lengths);
      if (l == NOT_KNOWN) {
         l = arithmetic_decode_target(MAXRULELEN_TARGET);
         arithmetic_decode(l, l + 1, MAXRULELEN_TARGET);
      }

      // read rule's right-hand side, symbol by symbol
      for (int j = 0; j < l; j ++) {
         int x = get_symbol();
         if (IS_NONTERMINAL(x))
            R[ix]->last()->insert_after(new symbols(R[CODE_TO_NONTERM(x)]));
         else {
            if (x == NOT_KNOWN) {
               x = arithmetic_decode_target(MINMAXTERM_TARGET);
               arithmetic_decode(x, x + 1, MINMAXTERM_TARGET);
               install_symbol(symbol, x);
             }
            R[ix]->last()->insert_after(new symbols(CODE_TO_TERM(x)));
         }
     }
     return n;
  }

  // other symbol

  else return i;
}

/**** Decompression entry point ****/

// Decompress compressed file.
void uncompress()
{
  extern int numbers;     // true - we output (terminal) symbol codes in decimal form, one per line; false - as characters

  R = (rules **) malloc(UNCOMPRESS_RSIZE * sizeof(rules *));

  start_compress(true);

  while (1) {
    int current = current_rule;

    // read a symbol
    int i = get_symbol();

    if (i == END_OF_FILE) break;
    else if (i == STOP_FORGETTING) forgetting = 0;
    // symbol is a yet unknown terminal
    else if (i == NOT_KNOWN)
    {
      int j = arithmetic_decode_target(MINMAXTERM_TARGET);
      arithmetic_decode(j, j + 1, MINMAXTERM_TARGET);
      install_symbol(symbol, j);

      cout << char(CODE_TO_TERM(j));
    }
    // symbol is a (known) terminal
    else if (IS_TERMINAL(i))
    {
      if (numbers) cout <<  int(CODE_TO_TERM(i)) << endl;
      else         cout << char(CODE_TO_TERM(i));
    }
    // symbol is a non-terminal
    else
    {
      int j = CODE_TO_NONTERM(i);
      // if we are "forgetting rules", non-terminal is followed
      if (i < current && forgetting) {
	// by keep index
        int keepi = decode(keep);

	// reproduce rule's full expansion, unless keep index says not to
        if (keepi != KEEPI_DUMMY) R[j]->reproduce();

	// delete rule from memory, if keep index says so
        if (keepi == KEEPI_NO || keepi == KEEPI_DUMMY) {
           delete_symbol(symbol, i);
           while (R[j]->first()->next() != R[j]->first()) delete R[j]->first();
           delete R[j];
        }
      }
      // we are not "forgetting rules", rule is not followed
      // by keep index, just reproduce
      else R[j]->reproduce();
    }
  }

  end_compress();
}
