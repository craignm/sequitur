/******************************************************************************
File:		arith.c

Authors: 	John Carpinelli   (johnfc@ecr.mu.oz.au)
	 	Wayne Salamonsen  (wbs@mundil.cs.mu.oz.au)
  		Lang Stuiver      (langs@cs.mu.oz.au)
  		Radford Neal      (radford@ai.toronto.edu)

Purpose:	Data compression using a revised arithmetic coding method.

Based on: 	A. Moffat, R. Neal, I.H. Witten, "Arithmetic Coding Revisted",
		Proc. IEEE Data Compression Conference, Snowbird, Utah, 
		March 1995.

		Low-Precision Arithmetic Coding Implementation by 
		Radford M. Neal



Copyright 1995 John Carpinelli and Wayne Salamonsen, All Rights Reserved.
Copyright 1996 Lang Stuiver, All Rights Reserved.

These programs are supplied free of charge for research purposes only,
and may not sold or incorporated into any commercial product.  There is
ABSOLUTELY NO WARRANTY of any sort, nor any undertaking that they are
fit for ANY PURPOSE WHATSOEVER.  Use them at your own risk.  If you do
happen to find a bug, or have modifications to suggest, please report
the same to Alistair Moffat, alistair@cs.mu.oz.au.  The copyright
notice above and this statement of conditions must remain an integral
part of each and every copy made of these files.

  $Log: arith.c,v $
  Revision 1.1  1996/08/07 01:34:11  langs
  Initial revision

 ************************************************************************

  The following arithmetic coding routines contain two different
methods for doing the required arithmetic.  The method must be specified at
compile time.  The first method uses multiplication and division, the
second simulates it with shifts and adds.
	The other decision is whether to specify the number of frequency and
code bits (F_BITS and B_BITS, respectively) at run time or compile time.
Fixing them at compile time gives improved performance, especially for the
shift/add version.

This is specified with the following flags (see the Makefile):

1. Multiplication and divison. (Fixed)
    -DMULT_DIV -DB_BITS=32 -DF_BITS=27

2. Multiplication and devision. (Variable)
    -DMULT_DIV -DVARY_NBITS

3. Shift/add arithmetic. (Fixed)
    -DB_BITS=32 -DF_BITS=27

4. Shift/add arithmetic. (Variable)
    -DVARY_NBITS

FIXING BITS:
Usually frequency bits and code bits would only be changed for testing,
and in a standard implementation might well be fixed at compile time.  The
advantage here is that the shift/add loop can be unrolled, without need
for testing and decrementing a counter variable.
	The loops are unrolled explicitly by calling an include file
unroll.i.  The compiler may well be able to unroll the loops automatically
when they are written to be detectable (with -funroll-loops in gcc, for
example).  Our aim here was to emphasise what is going on at the machine
level, and to ensure similar performance was obtained independent of the
C compiler used.

INPUT / OUTPUT:
Version 2.0.
  The input and output streams are now separate, with variables relating
to the streams prefixed "in_" and "out_" respectively.  This means that
decoding and encoding can be performed simultaneously.

DIFFERENCE FROM CONVENTIONAL ARITHMETIC CODING:
Approximate arithmetic allows a greater frequency range, at expense of
compression size.  The inefficiency is bounded by the maximum frequency and
when B_bits=32 and F_bits=27, for example, this inefficency is negligible.
If the shift/add optimisations are used, the total frequency count, t, must be
partially normalised, so that 2^{f-1} < t <= 2^f, where f is frequency bits,
the variable "F_bits".  This partial normalisation is done by stats.c, and
is the onus of the calling program if the coding routines are called
directly.  (This only applies to shift/add arithmetic; the original
multiplication/ division arithmetic will still work correctly with
arbitrary values for t).


FRUGAL_BITS option: See comments in arith.h.
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "arith.h"
#include "bitio.h"

#ifdef RCSID
static char
   rcsid[] = "$Id: arith.c,v 1.1 1996/08/07 01:34:11 langs Exp $";
#endif


/* Define coder_desc[] string, which describes the coder */

#define MULT_STR  "Arithmetic coding (multiply with "
#define SHIFT_STR "Arithmetic coding (Shift add with "
#define VARY_STR  "variable bits)"
#define FIXED_STR "fixed bits)"

#ifdef MULT_DIV
#  ifdef VARY_NBITS
     char *coder_desc =	MULT_STR VARY_STR;
#  else
     char *coder_desc =	MULT_STR FIXED_STR;
#  endif
#else	/* Shift add instead of multiply */
#  ifdef VARY_NBITS
     char *coder_desc =	SHIFT_STR VARY_STR;
#  else
     char *coder_desc =	SHIFT_STR FIXED_STR;
#  endif
#endif


#if defined(VARY_NBITS)
         int		B_bits = B_BITS;		/* Default values */
         int		F_bits = F_BITS;
  static code_value	Half;
  static code_value	Quarter;
#else
#	 define		Half		((code_value) 1 << (B_bits-1))
#	 define		Quarter		((code_value) 1 << (B_bits-2))
#endif

	/* Separate input and output */
/* Input decoding state */
static code_value	in_R;				/* code range */
static code_value	in_D;				/* = V-L (V offset)*/
static div_value	in_r;				/* normalized range */
#ifdef FRUGAL_BITS
static code_value	in_V;				/* Bitstream window */
#endif

/* Output encoding state */
static code_value	out_L;				/* lower bound */
static code_value	out_R;				/* code range */
static unsigned long	out_bits_outstanding;		/* follow bit count */


/*
 * BIT_PLUS_FOLLOW(b)
 * responsible for outputting the bit passed to it and an opposite number of
 * bit equal to the value stored in bits_outstanding
 *
 */
#define ORIG_BIT_PLUS_FOLLOW(b)		\
do                                      \
{ 	  			        \
    OUTPUT_BIT((b));           		\
    while (out_bits_outstanding > 0)	\
    { 					\
	OUTPUT_BIT(!(b));      		\
	out_bits_outstanding--;    	\
    } 	                		\
} while (0)


#ifdef FRUGAL_BITS

    int _ignore_first_bit = 1;

#  define BIT_PLUS_FOLLOW(x)		\
    do						\
    {						\
      if (_ignore_first_bit)			\
	_ignore_first_bit = 0;			\
      else					\
	ORIG_BIT_PLUS_FOLLOW(x);			\
    } while (0)

#else

#  define BIT_PLUS_FOLLOW(x)	ORIG_BIT_PLUS_FOLLOW(x)

#endif

/*
 * ENCODE_RENORMALISE
 * output code bits until the range has been expanded
 * to above QUARTER
 * With FRUGAL_BITS option, ignore first zero bit output
 * (a redundant zero will otherwise be emitted every time the encoder is
 * started)
 */
#define ENCODE_RENORMALISE		\
do {					\
    while (out_R <= Quarter)		\
    {					\
        if (out_L >= Half)		\
    	{				\
    	    BIT_PLUS_FOLLOW(1);		\
    	    out_L -= Half;		\
    	}				\
    	else if (out_L+out_R <= Half)	\
    	{				\
    	    BIT_PLUS_FOLLOW(0);		\
    	}				\
    	else 				\
    	{				\
    	    out_bits_outstanding++;	\
    	    out_L -= Quarter;		\
    	}				\
    	out_L <<= 1;			\
    	out_R <<= 1;			\
    }					\
} while (0)


/*
 * DECODE_RENORMALISE
 * input code bits until range has been expanded to
 * more than QUARTER. Mimics encoder.
 * FRUGAL_BITS option also keeps track of bitstream input so it can work out
 * exactly how many disambiguating bits the encoder put out (1,2 or 3).
 */
#ifdef FRUGAL_BITS
#define DECODE_RENORMALISE			\
do {                                            \
    while (in_R <= Quarter)                     \
    {                                           \
        in_R <<= 1;                             \
        in_V <<= 1;                             \
        ADD_NEXT_INPUT_BIT(in_D,B_bits);	\
	if (in_D & 1)				\
		in_V++;				\
    }                                           \
} while (0)
#else
#define DECODE_RENORMALISE			\
do {						\
    while (in_R <= Quarter)			\
    {						\
    	in_R <<= 1;				\
    	ADD_NEXT_INPUT_BIT(in_D,0);		\
    }						\
} while (0)
#endif


/*
 *
 * encode a symbol given its low, high and total frequencies
 *
 */
void arithmetic_encode(freq_value low, freq_value high, freq_value total)
{ 
  /* The following pseudocode is a concise (but slow due to arithmetic
   * calculations) description of what is calculated in this function.
   * Note that the division is done before the multiplication.  This is
   * one of the key points of this version of arithmetic coding.  This means
   * that much larger frequency values can be accomodated, but that the integer
   * ratio R/total (code range / frequency range) 
   * becomes a worse approximation as the total frequency count
   * is increased.  Therefore less than the whole code range will be
   * allocated to the frequency range.  The whole code range is used by
   * assigning the symbol at the end of the frequency range (where high==total)
   * this excess code range above and beyond its normal code range.  This
   * of course distorts the probabilities slightly, 
   * see the paper connected with this program for details and compression
   * results.
   *
   * Restricting the total frequency range means that the division and
   * multiplications can be done to low precision with shifts and adds.
   *
   * The following code describes this function:
   * (The actual code is only written less legibly to improve speed)
   *
   *    L += low*(R/total);			* Adjust low bound	*
   *
   *    if (high < total)			* 			*
   *    	R = (high-low) * (R/total);	* Restrict range	*
   *    else					* If symbol at end of	*
   *		R = R   -  low * (R/total);	*   range.  Restrict &	*
   *						*   allocate excess 	*
   * 						*   codelength to it.	*
   *    ENCODE_RENORMALISE;			* Expand code range	*
   *						*   and output bits	*
   *
   *    if (bits_outstanding > MAX_BITS_OUTSTANDING)
   *		abort();			* EXTREMELY improbable	*
   *						* (see comments below) *
   */

    code_value temp; 

#ifdef MULT_DIV
   {
    div_value out_r;			
    out_r = out_R/total;		/* Calc range:freq ratio */
    temp = out_r*low;			/* Calc low increment */
    out_L += temp;			/* Increase L */
    if (high < total)
	out_R = out_r*(high-low);	/* Restrict R */
    else
	out_R -= temp;			/* If at end of freq range */
					/* Give symbol excess code range */
   }
#else
  {
    code_value A, M;	/* A = numerator, M = denominator */
    code_value temp2;

    /*
     * calculate r = R/total, temp = r*low and temp2 = r*high
     * using shifts and adds  (r need not be stored as it is implicit in the
     * loop)
     */
    A = out_R;
    temp = 0;
    temp2 = 0;

#   ifdef VARY_NBITS
    {
        int i, nShifts;
	nShifts = B_bits - F_bits - 1;
	M = total << nShifts;
	   for (i = nShifts;; i--) 
	   {
	       if (A >= M) 
		{ 
		    A -= M; 
		    temp += low;
		    temp2 += high;
		}
		if (i == 0) break;
       	        A <<= 1; temp <<= 1; temp2 <<= 1;
    	   }
    }
#   else
	M = total << (B_bits - F_bits - 1);
	if (A >= M) { A -= M; temp += low; temp2 += high; }

#	define UNROLL_NUM	B_bits - F_bits - 1
#	define UNROLL_CODE						\
   	        A <<= 1; temp <<= 1; temp2 <<= 1;			\
		if (A >= M) { A -= M; temp += low; temp2 += high; }
#	include "unroll.i"	/* unroll the above code */

#   endif			/* Varying/nonvarying shifts */

    out_L += temp;
    if (high < total)
	out_R = temp2 - temp;
    else
	out_R -= temp;
  }
#endif

    ENCODE_RENORMALISE;

    if (out_bits_outstanding > MAX_BITS_OUTSTANDING)
    {
/* 
 * For MAX_BITS_OUTSTANDING to be exceeded is extremely improbable, but
 * it is possible.  For this to occur the COMPRESSED file would need to
 * contain a sequence MAX_BITS_OUTSTANDING bits long (eg: 2^31 bits, or
 * 256 megabytes) of all 1 bits or all 0 bits.  This possibility is
 * extremely remote (especially with an adaptive model), and can only
 * occur if the compressed file is greater than MAX_BITS_OUTSTANDING
 * long.  Assuming the compressed file is effectively random,
 * the probability for any 256 megabyte section causing an overflow
 * would be 1 in 2^(2^31).  This is a number approximately 600 million
 * digits long (decimal).
 * 
 */
	
	fprintf(stderr,"Bits_outstanding limit reached - File too large\n");
	exit(EXIT_FAILURE);
    }
}



/*
 * arithmetic decode_target()
 * decode the target value using the current total frequency
 * and the coder's state variables
 *
 * The following code describes this function:
 * The actual code is only written less legibly to improve speed,
 * including storing the ratio in_r = R/total for use by arithmetic_decode()
 *
 *  target = D / (R/total);	* D = V-L.  (Old terminology) 		*
 *				* D is the location within the range R	*
 *				* that the code value is located	*
 *				* Dividing by (R/total) translates it	*
 *				* to its correspoding frequency value	*
 *
 *				
 *  if (target < total) 	* The approximate calculations mean the *
 *	return target;		* encoder may have coded outside the	*
 *  else			* valid frequency range (in the excess	*
 *	return total-1;		* code range).  This indicates that the	*
 *				* symbol at the end of the frequency	*
 *				* range was coded.  Hence return end of *
 *				* range (total-1)			*
 *
 */
freq_value arithmetic_decode_target(freq_value total)
{
    freq_value target;
    
#ifdef MULT_DIV
    in_r = in_R/total;
    target = (in_D)/in_r;
#else 
   {	
    code_value A, M;		/* A = numerator, M = denominator */

    /* divide r = R/total using shifts and adds */
    A = in_R;
    in_r = 0;
#   ifdef  VARY_NBITS
    {
	int i, nShifts;
	nShifts = B_bits - F_bits - 1;
	M = total << nShifts;
	for (i = nShifts;; i--) 
	{
	  if (A >= M) { A -= M; in_r++; }
	  if (i == 0) break;
	  A <<= 1; in_r <<= 1;
	}
    
    /* divide D by r using shifts and adds */
    if (in_r < (1 << (B_bits - F_bits - 1)))
	nShifts = F_bits;
    else
	nShifts = F_bits - 1;
    A = in_D;
    M = in_r << nShifts;
    target = 0;
    for (i = nShifts;; i--) 
    {
        if (A >= M) 
	{ 
	    A -= M; 
	    target++; 
	}
	if (i == 0) break;
        A <<= 1; target <<= 1;
    }
  }
#   else
	M = total << ( B_bits - F_bits - 1 );
	if (A >= M) { A -= M; in_r++; }

#       define UNROLL_NUM	B_bits - F_bits - 1
#       define UNROLL_CODE			\
	   A <<= 1; in_r <<= 1;			\
	   if (A >= M) { A -= M; in_r++; }
#       include "unroll.i"

        A = in_D;
        target = 0;
        if (in_r < (1 << (B_bits - F_bits - 1)))
	    { M = in_r << F_bits; 
	       if (A >= M) { A -= M; target++; }	
	       A <<= 1; target <<= 1;		
	    }
        else
	    {
	        M = in_r << (F_bits - 1);
	    }

       if (A >= M) { A -= M; target++; }

#      define UNROLL_NUM  F_bits - 1
#      define UNROLL_CODE			\
           A <<= 1; target <<= 1;		\
           if (A >= M) { A -= M; target++; }	
#      include "unroll.i"
#   endif			/* vary or fixed shifts */
   }
#endif			/* Shift or multiply */

    return (target >= total ? total-1 : target);
}



/*
 * arithmetic_decode()
 *
 * decode the next input symbol
 *
 * The following code describes this function:
 * The actual code is only written less legibly to improve speed,
 * See the comments for arithmetic_encode() which is essentially the
 * same process.
 *
 * D -= low * (R/total);		* Adjust current code value 	   *
 *
 * if (high < total)
 *	R = (high-low) * (R/total);	* Adjust range 			   *
 * else
 *	R -= low * (R/total);		* End of range is a special case   *
 *
 * DECODE_RENORMALISE;			* Expand code range and input bits *
 *
 */

void arithmetic_decode(freq_value low, freq_value high, freq_value total)
{     
    code_value temp;

#ifdef MULT_DIV
    /* assume r has been set by decode_target */
    temp = in_r*low;
    in_D -= temp;
    if (high < total)
	in_R = in_r*(high-low);
    else
	in_R -= temp;
#else
{
    code_value temp2, M;
    
    /* calculate r*low and r*high using shifts and adds */
    temp = 0;
    temp2 = 0;

#   ifdef VARY_NBITS
    {
      int i, nShifts;
      M = in_r << F_bits;
      nShifts = B_bits - F_bits - 1;
      for (i = nShifts;; i--) 
      {
	  if (M & Half)
	  { 
	      temp += low;
	      temp2 += high;
	  }
	  if (i == 0) break;
          M <<= 1; temp <<= 1; temp2 <<= 1;
      }
    }
#   else
      M = in_r << F_bits;

      if (M & Half) { temp += low; temp2 += high; }

#     define UNROLL_NUM  B_bits - F_bits - 1
#     define UNROLL_CODE				\
        M <<= 1; temp <<= 1; temp2 <<= 1;		\
        if (M & Half) { temp += low; temp2 += high; }
#     include "unroll.i"

#   endif		/* Varying/not varying nshifts */

    in_D -= temp;
    if (high < total)
	in_R = temp2 - temp;
    else
	in_R -= temp;
 }
#endif		/* Shifts vs multiply */

    DECODE_RENORMALISE;

}



/*
 * binary_arithmetic_encode()
 * encode a binary symbol using specialised binary encoding
 * algorithm
 * 
 * The following code describes this function:
 * (The actual code is only written less legibly to improve speed)
 *
 * if (c0 < c1) { LPS = 0; cLPS = c0; }		* Determine Least Prob	*
 *	else	{ LPS = 1; cLPS = c1; }		* Symbol and its count	*
 *
 * rLPS = cLPS * (R/(c0+c1));		* Range of LPS			*
 *					* (Rest of range, including	*
 *					*  any "excess code range" is	*
 *					*  assigned to the MPS)		*
 * if (bit == LPS)
 *	{ L += R-rLPS; R = rLPS; }	* Adjust R and L depending	*
 * else 				* on symbol (less operations	*
 *	R -= rLPS;			* for most prob symbol)		*
 *
 * ENCODE_RENORMALISE;			* Expand range and output bits	*
 *
 * if (bits_outstanding > MAX_BITS_OUTSTANDING)
 *	abort();			* EXTREMELY unlikely (see notes	*
 *					* in arithmetic_encode() )	*
 *
 */
void binary_arithmetic_encode(freq_value c0, freq_value c1, int bit)
{
    int LPS;
    freq_value cLPS, rLPS;

    if (c0 < c1) 		/* From frequencies (c0 and c1) determine */ 
    {				/* least probable symbol (LPS) and its	*/
	LPS = 0;		/* count (cLPS)				*/
	cLPS = c0;
    } else {
	LPS = 1;
	cLPS = c1;
    }
#ifdef MULT_DIV
    {
     div_value out_r;
     out_r = out_R / (c0+c1);
     rLPS = out_r * cLPS;
    }
#else
   {	
    code_value numerator, denominator;

    numerator = out_R;
    rLPS = 0;

#  ifdef VARY_NBITS
   {
    int i, nShifts;
    nShifts = B_bits - F_bits - 1;
    denominator = (c0 + c1) << nShifts;
    for (i = nShifts;; i--) 
    {
	if (numerator >= denominator) 
	{ 
	    numerator -= denominator; 
	    rLPS += cLPS;
	}
	if (i == 0) break;
	numerator <<= 1; rLPS <<= 1;
    }
   }
#  else
        denominator = (c0 + c1) << (B_bits - F_bits - 1);
	if (numerator >= denominator) 
	{ 
	    numerator -= denominator; 
	    rLPS += cLPS;
	}
#       define UNROLL_NUM  B_bits - F_bits - 1
#       define UNROLL_CODE				\
	numerator <<= 1; rLPS <<= 1;			\
	if (numerator >= denominator) 			\
	{ 						\
	    numerator -= denominator; 			\
	    rLPS += cLPS;				\
	}
#       include "unroll.i"

#   endif			/* vary or fixed shifts */
   }

#endif				/* Mult or shift */

    if (bit == LPS) 
    {
	out_L += out_R - rLPS;
	out_R = rLPS;
    } else {
	out_R -= rLPS;
    }

    /* renormalise, as for arith_encode */
    ENCODE_RENORMALISE;

    if (out_bits_outstanding > MAX_BITS_OUTSTANDING)
    {
	fprintf(stderr,"Bits_outstanding limit reached - File too large\n");
	exit(EXIT_FAILURE);
    }
}



/*
 *
 * binary_arithmetic_decode()
 * decode a binary symbol given the frequencies of 1 and 0 for
 * the context
 *
 * Decoding is very similar to encoding
 * The following code describes this function:
 * (The actual code is only written less legibly to improve speed)
 *
 * if (c0 < c1) { LPS = 0; cLPS = c0; }		* Determine Least Prob	*
 *	else	{ LPS = 1; cLPS = c1; }		* Symbol and its count	*
 *
 * rLPS = cLPS * (R/(c0+c1));		* Range of LPS			*
 *					* (Rest of range, including	*
 *					*  any "excess code range" is	*
 *					*  assigned to the MPS)		*
 * if (D >= R-rLPS)
 *   { bit = LPS; D -= R-rLPS; R = rLPS; }  * Adjust R and D depending	*
 * else 				    * on symbol (less work	*
 *   { bit = 1-LPS;	       R -= rLPS;}  * for most prob symbol)	*
 *
 * DECODE_RENORMALISE;			* Expand range and input bits	*
 *
 * return bit;
 *
 */
int
binary_arithmetic_decode(freq_value c0, freq_value c1)
{
    int LPS;
    int bit;
    freq_value cLPS, rLPS;

    if (c0 < c1) 
    {
	LPS = 0;
	cLPS = c0;
    } else {
	LPS = 1;
	cLPS = c1;
    }
#ifdef MULT_DIV
    in_r = in_R / (c0+c1);
    rLPS = in_r * cLPS;
#else 
   {
    code_value numerator, denominator;

    numerator = in_R;
    in_r = 0;
    rLPS = 0;

#   ifdef VARY_NBITS
    {
    	    int i, nShifts;
	    nShifts = B_bits - F_bits - 1;
            denominator = (c0 + c1) << nShifts;
	    for (i = nShifts;; i--) 
	    {
		if (numerator >= denominator) 
		{ 
		    numerator -= denominator; 
		    rLPS += cLPS;
		}
		if (i == 0) break;
		numerator <<= 1; rLPS <<= 1;
	    }
    }
#   else
	        denominator = (c0 + c1) << (B_bits - F_bits - 1);
		if (numerator >= denominator) 
		{ 
		    numerator -= denominator; 
		    rLPS += cLPS;
		}
#		define UNROLL_NUM  B_bits - F_bits - 1
#		define UNROLL_CODE				\
		numerator <<= 1; rLPS <<= 1;			\
		if (numerator >= denominator) 			\
		{ 						\
		    numerator -= denominator; 			\
		    rLPS += cLPS;				\
		}
#		include "unroll.i"
#   endif
   }
#endif		/* MULT_DIV / non MULT_DIV */
    if ((in_D) >= (in_R-rLPS)) 
    {
	bit = LPS;
	in_D -= (in_R - rLPS);
	in_R = rLPS;
    } else {
	bit = (1-LPS);
	in_R -= rLPS;
    }

    /* renormalise, as for arith_decode */
    DECODE_RENORMALISE;

    return(bit);
}




/*
 *
 * start the encoder
 * With FRUGAL_BITS, ensure first bit (always 0) not actually output.
 *
 */
void start_encode(void)
{
#if defined(VARY_NBITS)
	/* Assume B_bits and F_bits have been selected (in main.c) */
	/* Set up Half, Quarter for this coding run	*/
    Half	  = ((code_value) 1 << (B_bits-1));
    Quarter 	  = ((code_value) 1 << (B_bits-2));
#endif

    out_L = 0;				/* Set initial coding range to	*/
    out_R = Half;			/* [0,Half)			*/
    out_bits_outstanding = 0;
#ifdef FRUGAL_BITS
    _ignore_first_bit = 1;		/* Don't ouput the leading 0	*/
#endif
}



#ifdef FRUGAL_BITS
/*
 * finish_encode()  (with FRUGAL_BITS)
 * finish encoding by outputting follow bits and one to three further
 * bits to make the last symbol unambiguous.
 *
 * Calculate the number of bits required:
 * value 	   = L rounded to "nbits" of precision (followed by zeros)
 * value + roundup = L rounded to "nbits" of precision (followed by ones)
 * Loop, increasing "nbits" until both the above values fall within [L,L+R),
 * then output these "nbits" of L
 */
void finish_encode(void)
{
  int nbits, i;
  code_value roundup, bits, value;
  for (nbits = 1; nbits <= B_bits; nbits++)
    {
	roundup = (1 << (B_bits - nbits)) - 1;
	bits = (out_L + roundup) >> (B_bits - nbits);
	value = bits << (B_bits - nbits);
	if (out_L <= value && value + roundup <= (out_L + (out_R - 1)) )
		break;
    }
  for (i = 1; i <= nbits; i++)        /* output the nbits integer bits */
        BIT_PLUS_FOLLOW(((bits >> (nbits-i)) & 1));
}
#else
/*
 * finish_encode() (without FRUGAL_BITS)
 * finish encoding by outputting follow bits and all the bits in L
 * (B_bits long) to make the last symbol unambiguous.  This also means the
 * decoder can read them harmlessly as it reads beyond end of what would
 * have been valid input.
 *
 */
void finish_encode(void)
{
  int nbits, i;
  code_value bits;

  nbits = B_bits;
  bits  = out_L;
  for (i = 1; i <= nbits; i++)        /* output the nbits integer bits */
        BIT_PLUS_FOLLOW(((bits >> (nbits-i)) & 1));
}
#endif

/* 
 * start_decode() start the decoder Fills the decode value in_D from the
 * bitstream.  If FRUGAL_BITS is defined only the first call reads in_D
 * from the bitstream.  Subsequent calls will assume the excess bits
 * that had been read but not used (sitting in in_V) are the start of
 * the next coding message, and it will put these into in_D.  (It also
 * initialises in_V to the start of the bitstream)
 * 
 * FRUGAL_BITS also means that the first bit (0) was not output, so only
 * take B_bits-1 from the input stream.  Since there are B_bits
 * "read-ahead" in the buffer, on second and subsequent calls
 * retrieve_excess_input_bits() is used and the last bit must be placed back
 * into the input stream.
 */
void 
start_decode(void)
{
 int i;
#if defined(VARY_NBITS)
	/* B_bits will have been selected */
    Half	  = ((code_value) 1 << (B_bits-1));
    Quarter 	  = ((code_value) 1 << (B_bits-2));
#endif

  in_D = 0;			/* Initial offset in range is 0 */
  in_R = Half;			/* Range = Half */

#ifdef FRUGAL_BITS
  {
    static int firstmessage = 1;
    if (firstmessage)
	{
	    for (i = 0; i < B_bits-1; i++)
		ADD_NEXT_INPUT_BIT(in_D, B_bits);
	}
    else
	{
	    in_D = retrieve_excess_input_bits();
	    unget_bit(in_D & 1);
	    in_D >>=1;
	}

    firstmessage = 0;
    in_V = in_D;
  }
#else
  for (i = 0; i<B_bits; i++)			/* Fill D */
	ADD_NEXT_INPUT_BIT(in_D, 0);
#endif

  if (in_D >= Half)
	{
	  fprintf(stderr,"Corrupt input file (start_decode())\n");
	  exit(EXIT_FAILURE);
	}
}

#ifdef FRUGAL_BITS
/*
 * finish_decode()  (with FRUGAL_BITS option)
 * finish decoding by consuming the disambiguating bits generated
 * by finish_encode (either 1, 2 or 3).  Calculated as for finish_encode()
 *
 * This will mean the coder will have read exactly B_bits
 * past end of valid coding output (these bits can be retrived with a call to
 * retrieve_excess_input_bits() )
 *
 */
void finish_decode(void)
{
  int nbits, i;
  code_value roundup, bits, value;
  code_value in_L;

  /* This gets us either the real L, or L + Half.  Either way, we can work
   * out the number of bit emitted by the encoder
   */
  in_L = (in_V & (Half-1))+ Half - in_D;

  for (nbits = 1; nbits <= B_bits; nbits++)
    {
        roundup = (1 << (B_bits - nbits)) - 1;
        bits = (in_L + roundup) >> (B_bits - nbits);
        value = bits << (B_bits - nbits);
        if (in_L <= value && value + roundup <= (in_L + (in_R - 1)) )
                break;
    }

    for (i = 1; i <= nbits; i++)
        {
        ADD_NEXT_INPUT_BIT(in_V, B_bits);
        }
}

/* retrieve_excess_input_bits():
 *   With FRUGAL_BITS defined, B_bits beyond valid coding output are read.
 *   It is these excess bits that are returned by calling this function.
 */
code_value retrieve_excess_input_bits(void)
{
	return (in_V & (Half + (Half-1)) );
}

#else

/* 
 * finish_decode()  (Without FRUGAL_BITS option)
 *
 * Throw away B_bits in buffer by doing nothing
 * (encoder wrote these for us to consume.)
 * (They were mangled anyway as we only kept V-L, and cannot get back to V)
 */
void finish_decode(void)
{
	/* No action */
}
#endif
