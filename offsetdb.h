#ifndef OFFSETDB_H
#define OFFSETDB_H

#include <stdio.h>
#include <stdint.h>

typedef uint32_t pipos_t;
#define PRpipos "u"
#define PIPOS_END  0
#define PIPOS_MORE UINTMAX_C(pipos_t)

struct offsetdb {
    FILE *db, *pi;
    int psize;
};

int  offsetdb_create(const char *dbfile, const char *pifile, int psize);
int  offsetdb_open(struct offsetdb *db, const char *index, const char *digits);
void offsetdb_close(struct offsetdb *db);
int  offsetdb_search(struct offsetdb *db, const char *pattern);

#endif
