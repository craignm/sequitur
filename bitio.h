/******************************************************************************
File:           bitio.h

Authors:        John Carpinelli   (johnfc@ecr.mu.oz.au)          
                Wayne Salamonsen  (wbs@mundil.cs.mu.oz.au)       
                Lang Stuiver      (langs@cs.mu.oz.au)

Purpose:        Data compression using a revised arithmetic coding method.
Based on:       A. Moffat, R. Neal, I.H. Witten, "Arithmetic Coding Revisted",
                Proc. IEEE Data Compression Conference, Snowbird, Utah, 
                March 1995.

                Low-Precision Arithmetic Coding Implementation by
                Radford M. Neal



Copyright 1996 Lang Stuiver, All Rights Reserved.
Based on work by
Copyright 1995 John Carpinelli and Wayne Salamonsen, All Rights Reserved.

******************************************************************************
 
  Bit and byte input output functions.
  Input/Output to stdin/stdout 1 bit at a time.
  Also byte i/o and fread/fwrite, so can keep a count of bytes read/written
   
  Once bit functions are used for either the input or output stream,
  byte based functions are NOT safe to use, unless a
  flush_{input,output}stream is first called. (Since bits are buffered a
  char at a time, while bytes are output immediately)
 
******************************************************************************/

#ifndef BITIO_H
#define BITIO_H


#define		BYTE_SIZE		8

/* As declared in bitio.c */
extern unsigned int	_bytes_input, _bytes_output;

extern int 		_in_buffer;		/* Input buffer	 	    */
extern unsigned char	_in_bit_ptr;		/* Input bit pointer 	    */
extern int		_in_garbage;		/* # of bytes read past EOF */

extern int		_out_buffer;		/* Output buffer 	    */
extern int		_out_bits_to_go;	/* Output bits in buffer    */

#ifndef FAST_BITIO
extern int		_bitio_tmp;		/* Used by i/o macros to    */
#endif						/* keep function ret values */


/*
 * OUTPUT_BIT(b)
 *
 * Outputs bit 'b' to stdout.  (Builds up a buffer, writing a byte
 * at a time.)
 *
 */

#define OUTPUT_BIT(b)				\
do {						\
   _out_buffer <<= 1;				\
   if (b)					\
	_out_buffer |= 1;			\
   _out_bits_to_go--;				\
   if (_out_bits_to_go == 0)			\
    {						\
	OUTPUT_BYTE(_out_buffer);		\
	_out_bits_to_go = BYTE_SIZE;		\
        _out_buffer = 0;			\
    }						\
} while (0)

/* 
 * ADD_NEXT_INPUT_BIT(v, garbage_bits)
 * 
 * Returns a bit from stdin, by shifting 'v' left one bit, and adding
 * next bit as lsb (possibly reading upto garbage_bits extra bits beyond
 * valid input)
 * 
 * garbage_bits:  Number of bits (to nearest byte) past end of file to
 * be allowed to 'read' before printing an error message and halting.
 * This is needed by our arithmetic coding module when the FRUGAL_BITS
 * option is defined, as upto B_bits extra bits may be needed to keep
 * the code buffer full (although the actual bitvalue is not important)
 * at the end of decoding.
 * 
 * The buffer is not shifted, instead a bit flag (_in_bit_ptr) is moved
 * to point to the next bit that is to be read.  When it is zero, the
 * next byte is read, and it is reset to point to the msb.
 * 
 */
#define ADD_NEXT_INPUT_BIT(v, garbage_bits)				\
do {									\
    if (_in_bit_ptr == 0)						\
    {									\
	_in_buffer = getchar();						\
	if (_in_buffer==EOF) 						\
	   {								\
		_in_garbage++;						\
		if ((_in_garbage-1)*8 >= garbage_bits)			\
		  {							\
		    fprintf(stderr,"Bad input file - attempted "	\
		 		   "read past end of file.\n");		\
		    exit(1);						\
		  }							\
	   }								\
	else								\
	   { _bytes_input++; }						\
	_in_bit_ptr = (1<<(BYTE_SIZE-1));				\
    }									\
    v = (v << 1);							\
    if (_in_buffer & _in_bit_ptr) v++;					\
    _in_bit_ptr >>= 1;							\
} while (0)


/*#define FAST_BITIO*/
/* Normally count input and output bytes so program can report stats
 * With FAST_BITIO set, no counting is maintained, which means file sizes
 * reported with the '-v' option will be meaningless, but should improve
 * speed slightly.
 */
#ifdef FAST_BITIO
#  define OUTPUT_BYTE(x)  putchar(x)
#  define INPUT_BYTE()    getchar()
#  define BITIO_FREAD(ptr, size, nitems)     fread(ptr, size, nitems, stdin)
#  define BITIO_FWRITE(ptr, size, nitems)    fwrite(ptr, size, nitems, stdout)
#else
#  define OUTPUT_BYTE(x)	( _bytes_output++, putchar(x) )

#  define INPUT_BYTE()	( _bitio_tmp = getchar(), 			\
			  _bytes_input += (_bitio_tmp == EOF ) ? 0 : 1, \
			  _bitio_tmp  )

#  define BITIO_FREAD(ptr, size, nitems)			\
	( _bitio_tmp = fread(ptr, size, nitems, stdin),		\
	  _bytes_input += _bitio_tmp * size,			\
	  _bitio_tmp )				/* Return result of fread */

#  define BITIO_FWRITE(ptr, size, nitems)				\
	( _bitio_tmp = fwrite(ptr, size, nitems, stdout),	\
	  _bytes_output += _bitio_tmp * size,			\
	  _bitio_tmp )				/* Return result of fwrite */
#endif

void startoutputtingbits(void);
void startinputtingbits(void);
void doneoutputtingbits(void);
void doneinputtingbits(void);
int bitio_bytes_in(void);
int bitio_bytes_out(void);

void unget_bit(int bit);

#endif		/* ifndef bitio_h */
