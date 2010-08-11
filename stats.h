/******************************************************************************
File:		stats.h

Authors: 	John Carpenelli   (johnfc@ecr.mu.oz.au)
	 	Wayne Salamonsen  (wbs@mundil.cs.mu.oz.au)

Purpose:	Data compression using a word-based model and revised 
		arithmetic coding method.

Based on: 	A. Moffat, R. Neal, I.H. Witten, "Arithmetic Coding Revisited",
		Proc. IEEE Data Compression Conference, Snowbird, Utah, 
		March 1995.


Copyright 1995 John Carpinelli and Wayne Salamonsen, All Rights Reserved.

These programs are supplied free of charge for research purposes only,
and may not sold or incorporated into any commercial product.  There is
ABSOLUTELY NO WARRANTY of any sort, nor any undertaking that they are
fit for ANY PURPOSE WHATSOEVER.  Use them at your own risk.  If you do
happen to find a bug, or have modifications to suggest, please report
the same to Alistair Moffat, alistair@cs.mu.oz.au.  The copyright
notice above and this statement of conditions must remain an integral
part of each and every copy made of these files.

******************************************************************************/
#ifndef STATS_H
#define STATS_H

/* 
   Note:  arith.h needs to have been previously included, as it has the
   freq_value definition.  Theoretically it should have been defined here,
   but it is easier to read and modify when the frequency and code
   value definitions are placed together, as they are interrelated.
 */

/* 
 * macros to add and remove the end '1' bit of a binary number 
 * using two's complement arithmetic
 */
#define BACK(i)			((i) & ((i) - 1))	
#define FORW(i)			((i) + ((i) & - (i)))

#define NOT_KNOWN		(-1)	/* attempt to code unknown symbol */
#define TOO_MANY_SYMBOLS	(-1)	/* could not install symbol */
#define NO_MEMORY		(-2)	/* install exceeded memory */

#define STATIC			0	/* context cannot grow- no escape */
#define	DYNAMIC			1	/* context may grow- escape needed */

/* memory used per symbol is a determined by noting that the Fenwick
 * structure used to store the symbols in is doubled as it is
 * realloc'ed.  The old structures (assuming not reusable) roughly
 * double the space used.  This doubling also means that almost half the
 * space (the new half) may never be used.  Together, this means, the
 * maximum cost of a "dynamic symbol" to memory requirements may be up
 * to 4 times that of a "static symbol" (the amount of memory it
 * actually takes to store the symbol).
 * Here we define MEM_PER_SYMBOL as this worst case, so that we can always
 * stay within any user specified memory limit.
 */
#define	MEM_PER_SYMBOL		(4 * sizeof(freq_value))	

#define MIN_INCR		1	/* minimum increment value */


/* context structure used to store frequencies */
typedef struct {
    int initial_size;			/* original length of context */
    int max_length, length;		/* length of tree and current length */
    freq_value nSingletons;		/* no. symbols with frequency=1 */
    int type;				/* context may be STATIC or DYNAMIC */
    int nSymbols;			/* count of installed symbols */
    freq_value total;			/* total of all frequencies */
    freq_value *tree;			/* Fenwick's binary index tree */
    freq_value incr;			/* current increment */
    int 	most_freq_symbol;
    freq_value	most_freq_count;
    freq_value	most_freq_pos;
} context;


/* context structure for binary contexts */
typedef struct {
    freq_value c0;			/* number of zeroes */
    freq_value c1;			/* number of ones */
    freq_value incr;			/* current increment used */
} binary_context;


extern char *stats_desc;


/* function prototypes */
context *create_context(int length, int type);
int install_symbol(context *pTree, int symbol);
void delete_symbol(context *pTree, int symbol);
int encode(context *pContext, int symbol);
int decode(context *pContext);
void purge_context(context *pContext);
binary_context *create_binary_context(void);
int binary_encode(binary_context *pContext, int bit);
int binary_decode(binary_context *pContext);

#endif
