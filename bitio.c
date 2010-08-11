/******************************************************************************
File:           bitio.c

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

These programs are supplied free of charge for research purposes only,
and may not sold or incorporated into any commercial product.  There is
ABSOLUTELY NO WARRANTY of any sort, nor any undertaking that they are
fit for ANY PURPOSE WHATSOEVER.  Use them at your own risk.  If you do
happen to find a bug, or have modifications to suggest, please report
the same to Alistair Moffat, alistair@cs.mu.oz.au.  The copyright
notice above and this statement of conditions must remain an integral
part of each and every copy made of these files.

******************************************************************************
 Input output module.  Inputs and outputs at bit, character, and
 fread/fwrite level.

 $Log: bitio.c,v $
 Revision 1.1  1996/08/07 01:34:11  langs
 Initial revision

******************************************************************************/

#include <stdio.h>
#include "bitio.h"

#ifdef RCSID
static char
    rcsid[] = "$Id: bitio.c,v 1.1 1996/08/07 01:34:11 langs Exp $"; 
#endif


/* 
 * The following variables are supposedly local, but actually global so they
 * can be referenced by macro
 */

unsigned int	_bytes_input = 0;
unsigned int	_bytes_output = 0;

int		_in_buffer;			/* I/O buffer */
unsigned char	_in_bit_ptr = 0;		/* bits left in buffer */
int		_in_garbage;			/* bytes read beyond eof */

int		_out_buffer;			/* I/O buffer */
int		_out_bits_to_go;		/* bits to fill buffer */

#ifndef FAST_BITIO
int		_bitio_tmp;			/* Used by some of the */
#endif						/* bitio.h macros */

/*
 *
 * initialize the bit output function
 *
 */
void startoutputtingbits(void)
{
    _out_buffer = 0;
    _out_bits_to_go = BYTE_SIZE;
}

/*
 *
 * start the bit input function
 *
 */
void startinputtingbits(void)
{
    _in_garbage = 0;	/* Number of bytes read past end of file */
    _in_bit_ptr = 0;	/* No valid bits yet in input buffer */
}



/*
 *
 * complete outputting bits
 *
 */
void doneoutputtingbits(void)
{
    if (_out_bits_to_go != BYTE_SIZE)
	OUTPUT_BYTE(_out_buffer << _out_bits_to_go);
    _out_bits_to_go = BYTE_SIZE;
}

/*
 *
 * complete inputting bits
 *
 */
void doneinputtingbits(void)
{
      _in_bit_ptr = 0;	      /* "Wipe" buffer (in case more input follows) */
}

/*
 * Number of bytes read with bitio functions.
 */
int bitio_bytes_in(void)
{
    return _bytes_input;
}

/*
 * Number of bytes written with bitio functions.
 */
int bitio_bytes_out(void)
{
    return _bytes_output;
}

/*
 * Return bit to input stream.
 * Only guaranteed to be able to backup by 1 bit.
 */
void unget_bit(int bit)
{
  _in_bit_ptr <<= 1;

  if (_in_bit_ptr == 0)
	_in_bit_ptr = 1;

  _in_buffer = _in_buffer & (_in_bit_ptr - 1);	/* Only keep bits still to */
						/* to be read.		   */
  if (bit)
	_in_buffer |= _in_bit_ptr; 		/* Replace bit		   */
}

