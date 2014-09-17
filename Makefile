CFLAGS = -std=c99 -Wall -O3
LDLIBS = -lsqlite3

pipattern : pipattern.o sqlite_index.o flat.o offsetdb.o

pipattern.o : pipattern.c flat.h sqlite_index.h
sqlite_index.o : sqlite_index.c sqlite_index.h
flat.o : flat.c flat.h
offsetdb.o : offsetdb.c offsetdb.h

.PHONY : clean distclean run

clean :
	$(RM) pipattern *.o

distclean : clean
	$(RM) pi.sqlite pi.sqlite-journal pi.offsetdb

run : pipattern
	./$^
