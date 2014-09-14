CFLAGS = -std=c99 -Wall -O3 -DSQLITE_THREADSAFE=0
LDLIBS = -ldl

pipattern : pipattern.o sqlite3.o

sqlite3.o : sqlite3.c sqlite3.h
pipattern.o : pipattern.c sqlite3.h

.PHONY : clean run

clean :
	$(RM) pipattern pipattern.o *.sqlite *-journal

run : pipattern
	./$^
