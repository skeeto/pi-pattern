CFLAGS = -std=c99 -Wall -O3

pipattern : pipattern.o flat.o offsetdb.o

pipattern.o : pipattern.c offsetdb.h flat.h
flat.o : flat.c flat.h
offsetdb.o : offsetdb.c offsetdb.h

.PHONY : clean distclean run

clean :
	$(RM) pipattern *.o

distclean : clean
	$(RM) pi.offsetdb

run : pipattern
	./$^
