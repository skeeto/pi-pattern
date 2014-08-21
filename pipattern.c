#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <getopt.h>
#include <locale.h>
#include <sqlite3.h>

/* Check return value for errors. */
inline void X(int r) {
    if (r != SQLITE_OK) {
        fprintf(stderr, "%s\n", sqlite3_errstr(r));
        exit(EXIT_FAILURE);
    }
}

const int COMMIT_STEP = 4096 * 256;

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

struct index {
    sqlite3 *db;
    sqlite3_stmt *count, *insert, *search, *begin, *commit;
};

void index_open(struct index *index, const char *file)
{
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

void index_load(struct index *index, const char *name, int max, bool quiet)
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
    if (!quiet) fprintf(stderr, "Loading data ...\n");
    for (int64_t position = 1; buffer[0]; position++) {
        if (position % COMMIT_STEP == 0) {
            index_exec(index->commit);
            index_exec(index->begin);
            if (!quiet)
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
    if (!quiet)
        fprintf(stderr, "Committing ...\n");
    index_exec(index->commit);

    /* Close up */
    if (!quiet) fprintf(stderr, "Creating index ...\n");
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

/* Boyer-Moore-Horspool search */
void flat_search(const char *file, const char *needle)
{
    FILE *pi = fopen(file, "r");
    if (pi == NULL) {
        perror(file);
        exit(EXIT_FAILURE);
    }

    /* Initialize data structures */
    size_t length = strlen(needle);
    unsigned char pattern[length];
    memcpy(pattern, needle, length);
    size_t bad_char_skip[256];
    for (size_t i = 0; i < 256; i++)
        bad_char_skip[i] = length;
    size_t last = length - 1;
    for (size_t i = 0; i < last; i++)
        bad_char_skip[pattern[i]] = last - i;

    /* Initialize haystack */
    size_t hlength = length + 8; // context
    unsigned char haystack[hlength + 1];
    haystack[hlength] = '\0';
    if (fread(haystack, hlength, 1, pi) != 1) {
        if (ferror(pi)) {
            perror(file);
            exit(EXIT_FAILURE);
        }
    }

    /* Perform search on stream */
    size_t position = 0;
    while (!feof(pi)) {
        for (size_t i = last; haystack[i] == pattern[i]; i--)
            if (i == 0)
                printf("%" PRIu64 ": %s\n", position - 1, haystack);
        size_t skip = bad_char_skip[haystack[last]];
        position += skip;
        memmove(haystack, haystack + skip, hlength - skip);
        if (fread(haystack + hlength - skip, skip, 1, pi) != 1) {
            if (ferror(pi))  {
                perror(file);
                exit(EXIT_FAILURE);
            }
        }
    }

    if (fclose(pi) != 0) {
        perror(file);
        exit(EXIT_FAILURE);
    }
}

void print_usage(const char *name, int ret)
{
    printf("Usage: %s [OPTION] [PATTERN]\n", name);
    printf("  -i <digits file>     UTF-8 pi digit listing (pi-billion.txt)\n");
    printf("  -d <index file>      SQLite database filename (pi.sqlite)\n");
    printf("  -n <length>          Maximum pattern length (larger index)\n");
    printf("  -f                   Do a straight string search (no index)\n");
    printf("  -q                   Don't print indexing progress\n");
    printf("  -h                   Print this message and exit\n");
    exit(ret);
}

int main(int argc, char **argv)
{
    /* Options */
    bool quiet = false;
    bool use_index = true;
    int max_length = 16;
    const char *datafile = "pi-billion.txt";
    const char *indexfile = "pi.sqlite";

    /* Parse options */
    setlocale(LC_NUMERIC, "");
    int opt;
    while ((opt = getopt(argc, argv, "d:i:n:qhf")) != -1) {
        switch (opt) {
        case 'i':
            datafile = optarg;
            break;
        case 'd':
            indexfile = optarg;
            break;
        case 'n':
            max_length = atoi(optarg);
            break;
        case 'q':
            quiet = true;
            break;
        case 'f':
            use_index = false;
            break;
        case 'h':
            print_usage(argv[0], EXIT_SUCCESS);
            break;
        default:
            print_usage(argv[0], EXIT_FAILURE);
        }
    }

    if (use_index) {
        struct index index;
        index_open(&index, indexfile);
        if (index_count(&index) == 0)
            index_load(&index, datafile, max_length, quiet);
        for (int i = optind; i < argc; i++)
            index_search(&index, argv[i]);
        index_close(&index);
    } else {
        for (int i = optind; i < argc; i++)
            flat_search(datafile, argv[i]);
    }
    return 0;

}
