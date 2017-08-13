/******************************************************************************
File:		arith.h

Authors: 	John Carpinelli   (johnfc@ecr.mu.oz.au)
	 	Wayne Salamonsen  (wbs@mundil.cs.mu.oz.au)
		Lang Stuiver	  (langs@cs.mu.oz.au)

Purpose:	Data compression using revised arithmetic coding method.

Based on: 	A. Moffat, R. Neal, I.H. Witten, "Arithmetic Coding Revisted",
		Proc. IEEE Data Compression Conference, Snowbird, Utah, 
		March 1995.

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

******************************************************************************/
#ifndef CODER_H
#define CODER_H

/* ================= USER ADJUSTABLE PARAMETERS =================== */

	/* Default B_bits and F_bits */

#ifndef B_BITS
#define		B_BITS		32
#endif

#ifndef F_BITS
#define		F_BITS		27
#endif

/* Change these types for different precision calculations.  They may affect
 * the speed of the arithmetic operations (multiplcation, division, shift,
 * etc).
 * The way the stats module is implemented, the type of freq_value
 * must be able to accomodate f_bits+1 bits, instead of f_bits, to avoid
 * overflows.  Ie: For an f_bits of up to 31, type freq_value must be 32 bits.
 */
typedef unsigned long   code_value;	/* B_BITS of precision */
typedef unsigned long	freq_value;	/* F_BITS+1 of precision */
typedef unsigned long	div_value;	/* B_BITS-F_BITS of precision */


/* MAX_BITS_OUTSTANDING is a bound on bits_outstanding
 * If bits_outstanding ever gets above this number (extremely unlikely)
 * the program will abort with an error message.  (See arith.c for details).
 */
#define 	MAX_BITS_OUTSTANDING	((unsigned long)1<<31)


/*#define FRUGAL_BITS*/

/* #define FRUGAL_BITS to enable the FRUGAL_BITS option:

  This option removes redundant bits from the start and end of coding.
  Our standard implementation is wasteful of bits, outputting 1 at the front,
  and about B (eg: 32) at the tail of the coding sequence which it doesn't
  need to.  This option saves those bits, but runs slightly slower.

  Instead of just keeping the difference value, V-L (code value - low value)
  in the decoder, frugal bits keeps track of the low value and code value
  separately, so that only the necessary (1 to 3) disambiguating bits need
  coding/decoding at the end of coding.  Although when the decoder discovers
  this, B_bits past valid coding output will have been read (possibly past
  end of file).  These bits can be recovered by calling
  retrieve_excess_input_bits() (for example, if another coding stream was
  about to start).  This saves B-3 to B-1 bits (eg:  With B=32, saving
  between 29 and 31 bits).  Decoding time is slightly slower, as in_V
  (bitstream window) is maintained.
  

  Another 1 bit is saved by observing that with the initial range of
  [0, 0.5) which is used, the first bit of each coding sequence is zero.
  This bit need not be transmitted.  This can be implemented efficiently
  in the decoder, but slows the encoder which checks each time a bit
  is output whether it is the first bit of a coding sequence.
  The encoder would not need to be slowed by this if coding always began at
  the start of a byte (simply increase _bits_to_go in bitio.c), but we
  are allowing for consecutive coding sequences (start_encode(); ...
  finish_encode(); pairs) in the same bitstream.

  The status of this option is not stored in the compressed data.  Thus it
  is up to the user to ensure that the FRUGAL_BITS option is the same when
  decompressing as when compressing.
 */

/* ================= END USER ADJUSTABLE PARAMETERS =================== */


/* Determine maximum bits allowed, based on size of the types used
 * to store them.  Also, that MAX_F_BITS <= MAX_B_BITS-2
 */

#define		MAX_B_BITS   (int)( sizeof(code_value) * 8)
#define		MAX_F_BITS   (int)((sizeof(freq_value)*8)-1 < MAX_B_BITS - 2\
				?  (sizeof(freq_value)*8)-1 : MAX_B_BITS - 2)

/* If varying bits, variables are B_bits, F_bits, Half and Quarter,
 *	otherwise #define them
 * These variables will be read (and possibly changed) by main.c and
 *  stats.c
 */

#if defined(VARY_NBITS)
    extern int		B_bits;
    extern int		F_bits;
#else
#   define		B_bits		B_BITS
#   define		F_bits  	F_BITS
#endif



extern char *coder_desc;


/* function prototypes */
void arithmetic_encode(freq_value l, freq_value h, freq_value t);
freq_value arithmetic_decode_target(freq_value t);
void arithmetic_decode(freq_value l, freq_value h, freq_value t);
void binary_arithmetic_encode(freq_value c0, freq_value c1, int bit);
int binary_arithmetic_decode(freq_value c0, freq_value c1);
void start_encode(void);
void finish_encode(void);
void start_decode(void);
void finish_decode(void);

#ifdef FRUGAL_BITS
extern int	_ignore_first_bit;
code_value retrieve_excess_input_bits(void);
#endif

#endif		/* ifndef arith.h */
