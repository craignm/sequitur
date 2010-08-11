/******************************************************************************
File: 		stats.c

Authors: 	John Carpinelli   (johnfc@ecr.mu.oz.au)
	 	Wayne Salamonsen  (wbs@mundil.cs.mu.oz.au)
		Lang Stuiver	  (langs@cs.mu.oz.au)

Purpose:	Data compression and revised arithmetic coding method.

Based on: 	P.M. Fenwick, "A new data structure for cumulative probability
		tables", Software- Practice and Experience, 24:327-336,
		March 1994.

		A. Moffat, R. Neal, I.H. Witten, "Arithmetic Coding Revisited",
		Proc. IEEE Data Compression Conference, Snowbird, Utah, 
		March 1995.


Copyright 1995 John Carpinelli and Wayne Salamonsen, All Rights Reserved.
Copyright 1996, Lang Stuiver.  All Rights Reserved.

These programs are supplied free of charge for research purposes only,
and may not sold or incorporated into any commercial product.  There is
ABSOLUTELY NO WARRANTY of any sort, nor any undertaking that they are
fit for ANY PURPOSE WHATSOEVER.  Use them at your own risk.  If you do
happen to find a bug, or have modifications to suggest, please report
the same to Alistair Moffat, alistair@cs.mu.oz.au.  The copyright
notice above and this statement of conditions must remain an integral
part of each and every copy made of these files.

******************************************************************************/
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "arith.h"
#include "stats.h"

#ifdef RCSID
static char rcsid[] = "$Id: stats.c,v 1.2 1996/10/03 01:27:11 langs Exp $";
#endif


#ifdef VARY_NBITS
    static freq_value	Max_frequency;
#else
#   define	Max_frequency   ((freq_value) 1 << F_bits)
#endif

/* MOST_PROB_AT_END:
 * Put most probable symbol at end of range (for more accurate approximations)
 * This option influences the compressed data, but is not recorded in the
 * data stream.  It is for experimental purposes and it is recommended
 * that it is left #defined.
 */
#define MOST_PROB_AT_END

#ifdef MOST_PROB_AT_END
    char *stats_desc = "Cumulative stats with Fenwick tree (MPS at front)";
#else
    char *stats_desc = "Cumulative stats with Fenwick tree";
#endif

static void get_interval(context *pContext,
		         freq_value *pLow, freq_value *pHigh, int symbol);
static void halve_context(context *pContext);


/* INCR_SYMBOL_PROB increments the specified symbol probability by the 'incr'
 * amount.  If the most probable symbol is maintined at the end of the coding
 * range (MOST_PROB_AT_END #defined), then both INCR_SYMBOL_PROB_ACTUAL and
 * INCR_SYMBOL_PROB_MPS are used.  Otherwise, just INCR_SYMBOL_PROB_ACTUAL
 * is used.
 */


/* INCR_SYMBOL_PROB_ACTUAL:  Increment 'symbol' in 'pContext' by inc1' */

#define INCR_SYMBOL_PROB_ACTUAL(pContext, symbol, inc1)			\
   freq_value _inc = (inc1);						\
   freq_value *_tree = pContext->tree;					\
   int i=symbol;                                  			\
					/* Increment stats */		\
   do {									\
        _tree[i] += _inc;						\
        i = FORW(i);							\
   } while (i<pContext->max_length);					\
   pContext->total += _inc;


/* 
 * INCR_SYMBOL_PROB_MPS: Update most frequent symbol, assuming 'symbol'
 * in 'pContext' was just incremented.
 */

/* Assumes _inc already set by macro above */
/* And _low and _high set before also */
#define INCR_SYMBOL_PROB_MPS(pContext, symbol)				\
 {					/* Update most_freq_symbol */	\
   if (symbol == pContext->most_freq_symbol)				\
	pContext->most_freq_count += _inc;				\
   else if (_high-_low+_inc > pContext->most_freq_count)		\
	{ pContext->most_freq_symbol = symbol;				\
	  pContext->most_freq_count = _high-_low+_inc;			\
	  pContext->most_freq_pos   = _low;				\
	}								\
   else if (symbol < pContext->most_freq_symbol)			\
	pContext->most_freq_pos += _inc;				\
 }

/* 
 * Define INCR_SYMBOL_PROB.  Definition depends on whether most probable
 * symbol needs to be remembered
 */

#ifdef MOST_PROB_AT_END
#  define INCR_SYMBOL_PROB(pContext, symbol, low1, high1, inc1)		\
   do {                                            			\
	freq_value _low = low1;						\
	freq_value _high = high1;					\
	INCR_SYMBOL_PROB_ACTUAL(pContext, symbol, inc1)			\
	INCR_SYMBOL_PROB_MPS   (pContext, symbol)			\
      } while (0)

#else
#  define INCR_SYMBOL_PROB(pContext, symbol, low1, high1, inc1)		\
   do {                                            			\
	INCR_SYMBOL_PROB_ACTUAL(pContext, symbol, inc1)			\
      } while (0)
#endif



/* 
 * Zero frequency probability is specified as a count out of the total
 * frequency count.  It is stored as the first item in the tree (item
 * 1).  Luckily, a Fenwick tree is defined such that we can read the
 * value of item 1 directly (pContext->tree[1]), although it cannot be
 * updated directly.  After each symbol is coded, adjust_zero_freq() is
 * called to ensure that the zero frequency probability stored in the
 * tree is still accurate (and changes it if it has changed).
 */

#define adjust_zero_freq(pContext)					     \
do { freq_value diff;							     \
	diff = ZERO_FREQ_PROB(pContext) - pContext->tree[1];		     \
	if (diff != 0)							     \
		INCR_SYMBOL_PROB(pContext, 1, 0, pContext->tree[1], diff);   \
   } while (0)


/* 
 * ZERO_FREQ_PROB defines the count for the escape symbol.  We
 * implemented a variation of the XC method (which we call AX).  We
 * create a special escape symbol, which we keep up to date with the
 * count of "number of singletons + 1".  To achieve this, but still be
 * efficient with static contexts, we falsely increment the number of
 * singletons at the start of modelling for dynamic contexts, and keep
 * it at 0 for static contexts.  This way, nSingletons is always our
 * zero frequency probability, without the need to check if the context
 * is static or dynamic (remember that this operation would be done for
 * each symbol coded).
 */

/* Zero frequency symbol probability for context `ctx' */
#define ZERO_FREQ_PROB(ctx)   ((freq_value)ctx->nSingletons)

void init_zero_freq(context *pContext)
{
	/* nSingletons is now actually nSingletons + 1, but this means
	   we can use nSingletons directly as zero_freq_prob (see above) */
    if (pContext->type  == DYNAMIC)
	pContext->nSingletons += pContext->incr;
    else
	pContext->nSingletons = 0;
}

/*
 *
 * create a new frequency table using a binary index tree
 * table may be STATIC or DYNAMIC depending on the type parameter
 * DYNAMIC tables may grow above their intial length as new symbols
 * are installed
 *
 * There may be some confusion about table size, so I will
 * discuss the variables 'max_length' and 'size', which refer to
 * the size of the Fenwick structure.  Fenwick structures of a given size
 * can hold 2^n - 1 items, or max_length - 1 items. In the range 
 * 1..max_length-1.
 * For example, if create_context is called with length=2, then max_length=4,
 * and range of valid items are 1..3.
 * (max_length, refers to the maximum length of the structure before it
 *  needs to be expanded)
 */

context *create_context(int length, int type)
{
    context	*pContext;
    int		i;
    int		size = 1;

#ifdef VARY_NBITS
    /* Ensure max frequency set up.  */
    Max_frequency = ((freq_value) 1 << F_bits);
#endif

    /*
     * increment length to accommodate the fact 
     * that symbol 0 is stored at pos 2 in the array.
     * (Escape symbol is at pos 1, pos 0 is not used).
     */
    length+=2;

    /* round length up to next power of two */
    while (size < length)
	size = size << 1;

    /* malloc context structure and array for frequencies */
    if (((pContext = (context *) malloc(sizeof(context))) == NULL) ||
	((pContext->tree = (freq_value *) malloc((size+1)*sizeof(freq_value)))
	 == NULL))
    {
	fprintf(stderr, "stats: not enough memory to create context\n");
	exit(EXIT_FAILURE);
    }
    pContext->initial_size = size;	/* save for purging later */
    pContext->length = 1;		/* current no. of symbols */
    pContext->total = 0;		/* total frequency */
    pContext->nSymbols = 1;		/* count of symbols */
    pContext->type = type;		/* is context DYNAMIC or STATIC */
    pContext->max_length = size;	/* no. symbols before growing */

    pContext->most_freq_symbol = -1;	/* Initially no most_freq_symbol */
    pContext->most_freq_count = 0;
    pContext->most_freq_pos = 0;
    
    /* initialise contents of tree array to zero */
    for (i = 0; i < pContext->max_length; i++)
	pContext->tree[i] = 0;
					/* increment is initially 2 ^ f */
    pContext->incr = (freq_value) 1 << F_bits; 
    pContext->nSingletons = 0;

    init_zero_freq(pContext);
    adjust_zero_freq(pContext);

    return pContext;	    		/* return a pointer to the context */
}


/*
 *
 * install a new symbol in a context's frequency table
 * returns 0 if successful, TOO_MANY_SYMBOLS or NO_MEMORY if install fails
 *
 */

void delete_symbol(context *pContext, int symbol)
{
    freq_value low, high;

    symbol+=2;

    get_interval(pContext, &low, &high, symbol);
    
    INCR_SYMBOL_PROB(pContext, symbol, low, high, low - high);
}

int install_symbol(context *pContext, int symbol)
{
    int i;
    freq_value low, high;

		/* Increment because first user symbol (symbol 0)
			is stored at array position 2 */
    symbol+=2;

    /* 
     * if new symbol is greater than current array length then double length 
     * of array 
     */	
    while (symbol >= pContext->max_length) 
    {
	pContext->tree = (freq_value *) 
	    realloc(pContext->tree, 
		    pContext->max_length * 2 * sizeof(freq_value));
	if (pContext->tree == NULL)
	{
	    fprintf(stderr, "stats: not enough memory to expand context\n");
	    return NO_MEMORY;
	}

	/* clear new part of table to zero */
	for (i=pContext->max_length; i<2*pContext->max_length; i++)
	    pContext->tree[i] = 0;
	
	/* 
	 * initialize new part by setting first element of top half
	 * to total of bottom half
	 * this method depends on table length being a power of two 
	 */
	pContext->tree[pContext->max_length] = pContext->total;
	pContext->max_length <<= 1;
    }

    /* check that we are not installing too many symbols */
    if (((pContext->nSymbols + 1) << 1) >= Max_frequency)
	/* 
	 * cannot install another symbol as all frequencies will 
	 * halve to one and an infinite loop will result
	 */
	return TOO_MANY_SYMBOLS;	       
	
    if (symbol > pContext->length)	/* update length if necessary */
	pContext->length = symbol;
    pContext->nSymbols++;		/* increment count of symbols */

    get_interval(pContext, &low, &high, symbol);

    /* update the number of singletons if context is DYNAMIC */
    INCR_SYMBOL_PROB(pContext, symbol, low, high, pContext->incr);
    if (pContext->type == DYNAMIC)
	pContext->nSingletons += pContext->incr;

    adjust_zero_freq(pContext);

    /* halve frequency counts if total greater than Max_frequency */
    while (pContext->total > Max_frequency)
	halve_context(pContext);

    return 0;
}



/*
 *
 * encode a symbol given its context
 * the lower and upper bounds are determined using the frequency table,
 * and then passed on to the coder
 * if the symbol has zero frequency, code an escape symbol and
 * return NOT_KNOWN otherwise returns 0
 *
 */
int encode(context *pContext, int symbol)
{
    freq_value low, high, low_w, high_w;

    symbol+=2;
    if ((symbol > 0) && (symbol < pContext->max_length))
	{
	if (pContext->most_freq_symbol == symbol)
		{
		  low = pContext->most_freq_pos;
		  high = low + pContext->most_freq_count;
		}
	    else
	        get_interval(pContext, &low, &high, symbol);
	}
    else
	low = high = 0;
	
    if (low == high)
      {
	if (ZERO_FREQ_PROB(pContext) == 0) 
	{
	    fprintf(stderr,
		"stats: cannot code zero-probability novel symbol");
	    abort();
	    exit(EXIT_FAILURE);
	}
	/* encode the escape symbol if unknown symbol */
	symbol = 1;
	if (pContext->most_freq_symbol == 1)
		{
		low = pContext->most_freq_pos;
		high = low + pContext->most_freq_count;
		}
	   else
		get_interval(pContext, &low, &high, symbol);
      }

	/* Shift high and low if required so that most probable symbol
 	 * is at the end of the range
	 * (Shifted values are low_w, and high_w, as original values
	 * are needed when updating the stats)
	 */

#ifdef MOST_PROB_AT_END
    if (symbol > pContext->most_freq_symbol)
	{
	    low_w  = low  - pContext->most_freq_count;
	    high_w = high - pContext->most_freq_count;
	}
    else if (symbol == pContext->most_freq_symbol)
	{
	    low_w  = pContext->total - pContext->most_freq_count;
	    high_w = pContext->total;
	}
    else
	{
	    low_w  = low;
	    high_w = high;
	}
#else
	low_w  = low;
	high_w = high;
#endif

    /* call the coder with the low, high and total for this symbol
     * (with low_w, high_w:  Most probable symbol moved to end of range)
     */

    arithmetic_encode(low_w, high_w, pContext->total);

    if (symbol != 1)		/* If not the special ESC / NOT_KNOWN symbol */
    {
	/* update the singleton count if symbol was previously a singleton */
	if (pContext->type == DYNAMIC && high-low == pContext->incr)
		pContext->nSingletons -= pContext->incr;

	/* increment the symbol's frequency count */
	INCR_SYMBOL_PROB(pContext, symbol, low, high, pContext->incr);
    }

    adjust_zero_freq(pContext);

    while (pContext->total > Max_frequency)
	halve_context(pContext);

    if (symbol == 1) return NOT_KNOWN;
    return 0;
}




/*
 *
 * decode function is passed a context, and returns a symbol
 *
 */
int 
decode(context *pContext)
{
    int	symbol;
    freq_value low, high, mid, target;
    freq_value total = pContext->total;
 
    target = arithmetic_decode_target(total);

#ifdef MOST_PROB_AT_END
	/* Check if most probable symbol (shortcut decode)
	 */
    if (target >= total - pContext->most_freq_count)
	{ arithmetic_decode( total - pContext->most_freq_count,
		 	     total,
			     total);
	  symbol = pContext->most_freq_symbol;
	  low = pContext->most_freq_pos;
	  high = low + pContext->most_freq_count;
	}
    else
	/* Not MPS, have to decode slowly */
  {
	if (target >= pContext->most_freq_pos)
	    target += pContext->most_freq_count;

#endif

    symbol = 0; low = 0;
    mid = pContext->max_length >> 1;		/* midpoint is half length */
    {
      freq_value *tree_pointer = &(pContext->tree[symbol]);
      /* determine symbol from target value */
      while (mid > 0)
      {
	if (tree_pointer[mid] + low <= target)
	{
	    low += tree_pointer[mid];
	    tree_pointer += mid;
	}
	mid >>= 1;
      }
      symbol = tree_pointer - pContext->tree;  /* Pointer arithmetic */
    }
    symbol++;	/* Symbol was greatest symbol less than target, now symbol
		   pointed to in target range */

/* Since already have the lower bound (low), need only find the
 * upper bound (high) as the difference, don't need to work it
 * out from scratch.
 * (Calling get_interval(pContext, &low, &high, symbol) would work out
 * both, duplicating the work of finding low)
 */
    if (symbol & 1)
		high = low + pContext->tree[symbol];
	else
	  {
		int parent, sym_1;
		freq_value *tree = pContext->tree;
		parent = BACK(symbol);
		high = low;
		sym_1 = symbol - 1;
		do
		{
		  high -= tree[sym_1];
		  sym_1 = BACK(sym_1);
		} while (sym_1 != parent);
		high += tree[symbol];
	  }

#ifdef MOST_PROB_AT_END
    if (low >= pContext->most_freq_pos)  /* Ie: Was moved */
	arithmetic_decode(low  - pContext->most_freq_count,
			  high - pContext->most_freq_count,
			  total);
    else
#endif
	arithmetic_decode(low, high, total);

#ifdef MOST_PROB_AT_END
  }  /* If not MPS */
#endif

    /* update the singleton count if symbol was previously a singleton */
  if (symbol != 1)
    {
      if (pContext->type == DYNAMIC && high-low == pContext->incr)
	    pContext->nSingletons -= pContext->incr;

      /* increment the symbol's frequency count */
      INCR_SYMBOL_PROB(pContext, symbol, low, high, pContext->incr);
    }


    adjust_zero_freq(pContext);

    /* halve all frequencies if necessary */
    while (pContext->total > Max_frequency)
	halve_context(pContext);

    if (symbol == 1) return NOT_KNOWN;
    return symbol-2;
}



/*
 *
 * get the low and high limits of the frequency interval
 * occupied by a symbol.
 * this function is faster than calculating the upper bound of the two 
 * symbols individually as it exploits the shared parents of s and s-1.
 *
 */
static void 
get_interval(context *pContext, freq_value *pLow, freq_value *pHigh, int symbol)
{
    freq_value low, high, shared, parent;
    freq_value *tree = pContext->tree;

    /* calculate first part of high path */
    high = tree[symbol];
    parent = BACK(symbol);
    
    /* calculate first part of low path */
    symbol--;
    low = 0;
    while (symbol != parent)
    {
	low += tree[symbol];
	symbol = BACK(symbol);
    }

    /* sum the shared part of the path back to root */
    shared = 0;
    while (symbol > 0)
    {
	shared += tree[symbol];
	symbol = BACK(symbol);
    }
    *pLow = shared+low;
    *pHigh = shared+high;
}
 

/*
 *
 * halve_context is responsible for halving all the frequency counts in a 
 * context.
 * halves context in linear time by keeping track of the old and new 
 * values of certain parts of the array
 * also recalculates the number of singletons in the new halved context.
 *
 * It ensures the most probable symbol size and range stay updated.
 * If halving the context gives rise to a sudden drop in the
 * ZERO_FREQ_PROB(), and if it was the MPS, it will stay recorded as the most
 * probable symbol even if it isn't.  This may cause slight compression
 * inefficiency.  (The ZERO_FREQ_PROB() as implemented does not have this
 * characteristic, and in this case the inefficiency cannot occur)
 */

static void
halve_context(context *pContext)
{
    int	i, zero_count, temp;
    freq_value old_values[MAX_F_BITS], new_values[MAX_F_BITS];
    freq_value sum_old, sum_new;
    freq_value *tree = pContext->tree;
    freq_value incr;

    pContext->incr = (pContext->incr + MIN_INCR) >> 1;	/* halve increment */
    if (pContext->incr < MIN_INCR) pContext->incr = MIN_INCR;
    pContext->nSingletons = incr = pContext->incr;
    for (i = 1; i < pContext->max_length; i++)
    {

	/* work out position to store element in old and new values arrays */
	zero_count = 0;
	for (temp = i; !(temp&1); temp >>= 1)
	    zero_count++;

	/* move through context halving as you go */
	old_values[zero_count] = tree[i];
	sum_old = 0;
	sum_new = 0;
	for (temp = zero_count-1; temp >=0; temp--)
	{
	    sum_old += old_values[temp];
	    sum_new += new_values[temp];
	}
	tree[i] -= sum_old;
	pContext->total -= (tree[i]>>1);
	tree[i] -= (tree[i]>>1);
	if (tree[i] == incr && i != 1)
	    pContext->nSingletons += incr;
	tree[i] += sum_new;
	      
	new_values[zero_count] = tree[i];
    }

     if (pContext->type == STATIC)
	pContext->nSingletons = 0;

	/* Recalc new most_freq_symbol info if it exists
	 * (since roundoff may mean not exactly
	 * half of previous value)
	 */


    if (pContext->most_freq_symbol!=-1) 
      { freq_value low, high;
        get_interval(pContext, &low, &high, pContext->most_freq_symbol);
        pContext->most_freq_count = high-low;
        pContext->most_freq_pos = low;
      }

    adjust_zero_freq(pContext);

}


/*
 *
 * free memory allocated for a context and initialize empty context
 * of original size
 *
 */
void 
purge_context(context *pContext)
{
    int i;

    free(pContext->tree);
    
    /* malloc new tree of original size */
    if ((pContext->tree = (freq_value *)malloc((pContext->initial_size + 1)
					    * sizeof(freq_value))) == NULL)
    {
	fprintf(stderr, "stats: not enough memory to create context\n");
	exit(EXIT_FAILURE);
    }
    pContext->length = 1;
    pContext->total = 0;
    pContext->nSymbols = 1;		/* Start with escape symbol */

    pContext->most_freq_symbol = -1;	/* Indicates no such symbol */
    pContext->most_freq_count = 0;
    pContext->most_freq_pos = 0;

    pContext->max_length = pContext->initial_size;
    for (i = 0; i < pContext->initial_size; i++)
	pContext->tree[i] = 0;
				      /* increment is initially 2 ^ f */
    pContext->incr = (freq_value) 1 << F_bits;
    pContext->nSingletons = 0;

    init_zero_freq(pContext);
    adjust_zero_freq(pContext);
}

/******************************************************************************
*
* functions for binary contexts
*
******************************************************************************/

/*
 *
 * create a binary_context
 * binary contexts consist of two counts and an increment which
 * is normalized
 *
 */
binary_context *create_binary_context(void)
{
    binary_context *pContext;

#ifdef VARY_NBITS
    Max_frequency = ((freq_value) 1 << F_bits);
#endif

    pContext = (binary_context *) malloc(sizeof(binary_context));
    if (pContext == NULL)
    {
	fprintf(stderr, "stats: not enough memory to create context\n");
	exit(EXIT_FAILURE);
    }
					    /* start with incr=2^(f-1) */
    pContext->incr = (freq_value) 1 << (F_bits - 1);
    pContext->c0 = pContext->incr;
    pContext->c1 = pContext->incr;
    return pContext;
}



/*
 *
 * encode a binary symbol using special binary arithmetic
 * coding functions
 * returns 0 if successful
 *
 */
int
binary_encode(binary_context *pContext, int bit)
{
    binary_arithmetic_encode(pContext->c0, pContext->c1, bit);

    /* increment symbol count */
    if (bit == 0)
	pContext->c0 += pContext->incr;
    else
	pContext->c1 += pContext->incr;

    /* halve frequencies if necessary */
    if (pContext->c0 + pContext->c1 > Max_frequency)
    {
	pContext->c0 = (pContext->c0 + 1) >> 1;
	pContext->c1 = (pContext->c1 + 1) >> 1;
	pContext->incr = (pContext->incr + MIN_INCR) >> 1;
    }
    return 0;
}	



/*
 *
 * decode a binary symbol using specialised binary arithmetic
 * coding functions
 *
 */
int
binary_decode(binary_context *pContext)
{
    int bit;

    bit = binary_arithmetic_decode(pContext->c0, pContext->c1);

    /* increment symbol count */
    if (bit == 0)
	pContext->c0 += pContext->incr;
    else
	pContext->c1 += pContext->incr;

    /* halve frequencies if necessary */
    if (pContext->c0 + pContext->c1 > Max_frequency)
    {
	pContext->c0 = (pContext->c0 + 1) >> 1;
	pContext->c1 = (pContext->c1 + 1) >> 1;
	pContext->incr = (pContext->incr + MIN_INCR) >> 1;
    }    
    return bit;
}
