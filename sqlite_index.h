#ifndef SQLITE_INDEX_H
#define SQLITE_INDEX_H

#include <stdint.h>
#include <stdbool.h>
#include <sqlite3.h>

struct index {
    sqlite3 *db;
    sqlite3_stmt *count, *insert, *search, *begin, *commit;
    bool quiet;
};

void    index_open(struct index *index, const char *file);
void    index_close(struct index *index);
int64_t index_count(struct index *index);
void    index_load(struct index *index, const char *name, int max);
void    index_search(struct index *index, const char *pattern);

#endif
