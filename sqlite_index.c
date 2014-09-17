#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <sqlite3.h>
#include "sqlite_index.h"

/* Check return value for errors. */
inline void X(int r) {
    if (r != SQLITE_OK) {
        #ifdef sqlite_errstr
        fprintf(stderr, "%s\n", sqlite3_errstr(r));
        #else
        fprintf(stderr, "error: sqlite3\n");
        #endif
        exit(EXIT_FAILURE);
    }
}

static const int COMMIT_STEP = 4096 * 256;

static const char *SQL_BUSY =
    "PRAGMA busy_timeout = 10000";
static const char *SQL_DELETE =
    "DELETE FROM digits";
static const char *SQL_COUNT =
    "SELECT count(position) FROM digits";
static const char *SQL_CREATE =
    "CREATE TABLE IF NOT EXISTS digits"
    "(position INTEGER PRIMARY KEY, sequence TEXT NOT NULL)";
static const char *SQL_INDEX =
    "CREATE INDEX sequence_index ON digits (sequence, position)";
static const char *SQL_INSERT =
    "INSERT INTO digits (position, sequence) VALUES (?, ?)";
static const char *SQL_SEARCH =
    "SELECT position, sequence FROM digits WHERE sequence LIKE ?";

void index_open(struct index *index, const char *file)
{
    index->quiet = false;
    X(sqlite3_open(file, &index->db));
    X(sqlite3_exec(index->db, SQL_BUSY, NULL, NULL, NULL));
    X(sqlite3_exec(index->db, SQL_CREATE, NULL, NULL, NULL));
    X(sqlite3_exec(index->db, "PRAGMA page_size = 65536", NULL, NULL, NULL));
    X(sqlite3_prepare_v2(index->db, SQL_COUNT, -1, &index->count, NULL));
    X(sqlite3_prepare_v2(index->db, SQL_INSERT, -1, &index->insert, NULL));
    X(sqlite3_prepare_v2(index->db, SQL_SEARCH, -1, &index->search, NULL));
    X(sqlite3_prepare_v2(index->db, "BEGIN", -1, &index->begin, NULL));
    X(sqlite3_prepare_v2(index->db, "COMMIT", -1, &index->commit, NULL));
}

void index_close(struct index *index)
{
    X(sqlite3_finalize(index->count));
    X(sqlite3_finalize(index->insert));
    X(sqlite3_finalize(index->search));
    X(sqlite3_finalize(index->begin));
    X(sqlite3_finalize(index->commit));
    X(sqlite3_close(index->db));
}

inline void index_exec(sqlite3_stmt *stmt)
{
    sqlite3_step(stmt);
    X(sqlite3_reset(stmt));
}

int64_t index_count(struct index *index)
{
    sqlite3_step(index->count);
    int64_t count = sqlite3_column_int64(index->count, 0);
    X(sqlite3_reset(index->count));
    return count;
}

void index_load(struct index *index, const char *name, int max)
{
    FILE *pi = fopen(name, "r");
    if (pi == NULL) {
        perror(name);
        exit(EXIT_FAILURE);
    }

    /* Set up table */
    X(sqlite3_exec(index->db, SQL_DELETE, NULL, NULL, NULL));

    /* Init buffer */
    char buffer[max + 1];
    buffer[max] = '\0';
    getc(pi); // skip
    if (fread(buffer, max, 1, pi) != 1) {
        perror(name);
        exit(EXIT_FAILURE);
    }

    /* Walk pi */
    X(sqlite3_exec(index->db, "PRAGMA journal_mode = OFF", NULL, NULL, NULL));
    index_exec(index->begin);
    if (!index->quiet) fprintf(stderr, "Loading data ...\n");
    for (int64_t position = 1; buffer[0]; position++) {
        if (position % COMMIT_STEP == 0) {
            index_exec(index->commit);
            index_exec(index->begin);
            if (!index->quiet)
                fprintf(stderr, "\033[FLoaded %'" PRIu64 " digits...\n",
                        position);
        }
        memmove(buffer, buffer + 1, max - 1);
        int c = getc(pi);
        buffer[max - 1] = c == EOF ? '\0' : c;
        X(sqlite3_bind_int64(index->insert, 1, position));
        X(sqlite3_bind_text(index->insert, 2, buffer, -1, SQLITE_TRANSIENT));
        index_exec(index->insert);
    }
    if (!index->quiet)
        fprintf(stderr, "Committing ...\n");
    index_exec(index->commit);

    /* Close up */
    if (!index->quiet) fprintf(stderr, "Creating index ...\n");
    X(sqlite3_exec(index->db, SQL_INDEX, NULL, NULL, NULL));
    if (fclose(pi) != 0) {
        perror(name);
        exit(EXIT_FAILURE);
    }
}

void index_search(struct index *index, const char *pattern)
{
    size_t length = strlen(pattern);
    char glob[length + 2];
    strcpy(glob, pattern);
    glob[length + 0] = '%';
    glob[length + 1] = '\0';
    X(sqlite3_bind_text(index->search, 1, glob, -1, SQLITE_STATIC));
    while (sqlite3_step(index->search) == SQLITE_ROW) {
        int64_t p = sqlite3_column_int64(index->search, 0);
        const unsigned char *s = sqlite3_column_text(index->search, 1);
        printf("%" PRIu64 ": %s\n", p, s);
    }
    X(sqlite3_reset(index->search));
}
