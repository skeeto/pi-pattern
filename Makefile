CFLAGS = -std=c99 -Wall -O3
LDLIBS = -lsqlite3

pipattern : pipattern.c

.PHONY : clean run

clean :
	$(RM) pipattern *.sqlite *-journal

run : pipattern
	./$^
