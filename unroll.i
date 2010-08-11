/*  unroll.i
  Used for loop unrolling, duplicates the given code fragment a specified
  number of times.
  ---------------------------------------------------------- 
  Precondition:
	#define UNROLL_NUM	Number of repetitions
	#define UNROLL_CODE	Code to repeat
  To use:
	#include "unroll.i"
  Result:
	The code UNROLL_CODE repeated UNROLL_NUM times.
  ---------------------------------------------------------- 

  For example:
        The program fragment:
		...
		i=0;
		#define UNROLL_NUM 3
		#define UNROLL_CODE  printf("%i",i++);
		#include "unroll.i"
		...

	is equivalent to:
		...
		i=0;
		printf("%i",i++);
		printf("%i",i++);
		printf("%i",i++);
		...
  ---------------------------------------------------------- 
  If the number of times the code fragment is to be repeated is greater
  than a certain number (here, 32), then a loop is added to repeat the code
  fragment as required.  
 */


#if UNROLL_NUM > 0
  UNROLL_CODE
#endif
#if UNROLL_NUM > 1
  UNROLL_CODE
#endif
#if UNROLL_NUM > 2
  UNROLL_CODE
#endif
#if UNROLL_NUM > 3
  UNROLL_CODE
#endif
#if UNROLL_NUM > 4
  UNROLL_CODE
#endif
#if UNROLL_NUM > 5
  UNROLL_CODE
#endif
#if UNROLL_NUM > 6
  UNROLL_CODE
#endif
#if UNROLL_NUM > 7
  UNROLL_CODE
#endif
#if UNROLL_NUM > 8
  UNROLL_CODE
#endif
#if UNROLL_NUM > 9
  UNROLL_CODE
#endif
#if UNROLL_NUM > 10
  UNROLL_CODE
#endif
#if UNROLL_NUM > 11
  UNROLL_CODE
#endif
#if UNROLL_NUM > 12
  UNROLL_CODE
#endif
#if UNROLL_NUM > 13
  UNROLL_CODE
#endif
#if UNROLL_NUM > 14
  UNROLL_CODE
#endif
#if UNROLL_NUM > 15
  UNROLL_CODE
#endif
#if UNROLL_NUM > 16
  UNROLL_CODE
#endif
#if UNROLL_NUM > 17
  UNROLL_CODE
#endif
#if UNROLL_NUM > 18
  UNROLL_CODE
#endif
#if UNROLL_NUM > 19
  UNROLL_CODE
#endif
#if UNROLL_NUM > 20
  UNROLL_CODE
#endif
#if UNROLL_NUM > 21
  UNROLL_CODE
#endif
#if UNROLL_NUM > 22
  UNROLL_CODE
#endif
#if UNROLL_NUM > 23
  UNROLL_CODE
#endif
#if UNROLL_NUM > 24
  UNROLL_CODE
#endif
#if UNROLL_NUM > 25
  UNROLL_CODE
#endif
#if UNROLL_NUM > 26
  UNROLL_CODE
#endif
#if UNROLL_NUM > 27
  UNROLL_CODE
#endif
#if UNROLL_NUM > 28
  UNROLL_CODE
#endif
#if UNROLL_NUM > 29
  UNROLL_CODE
#endif
#if UNROLL_NUM > 30
  UNROLL_CODE
#endif
#if UNROLL_NUM > 31
  UNROLL_CODE
#endif

#if UNROLL_NUM > 32
  {  int __i7;
    for (__i7=0; __i7 < UNROLL_NUM-32; __i7++)
	{
		UNROLL_CODE
	}
  }
#endif

#undef UNROLL_NUM
#undef UNROLL_CODE
