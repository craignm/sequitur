.PHONY: clean

CFLAGS = -O3

all:	sequitur sequitur_simple

sequitur: sequitur.o classes.o compress.o arith.o bitio.o stats.o
	g++ $(CFLAGS) -o sequitur sequitur.o classes.o compress.o arith.o bitio.o stats.o

sequitur_simple: sequitur_simple.cc
	g++ $(CFLAGS) -o sequitur_simple sequitur_simple.cc

%.o: %.cc classes.h
	g++ -DPLATFORM_UNIX $(CFLAGS) -c $*.cc

arith.o: arith.c arith.h bitio.h unroll.i
	gcc $(CFLAGS) -c arith.c

bitio.o: bitio.c bitio.h
	gcc $(CFLAGS) -c bitio.c

stats.o: stats.c arith.h stats.h
	gcc $(CFLAGS) -c stats.c

test:
	make; ./test.pl
force:
	touch *.cc *.c; make

clean:
	rm *.o
