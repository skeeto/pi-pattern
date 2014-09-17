#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include "offsetdb.h"

#define max(a, b) ((a) < (b) ? (b) : (a))
#define min(a, b) ((a) < (b) ? (a) : (b))

struct pstack {
    pipos_t *ptrs;
    size_t size, fill;
};

static int pstack_init(struct pstack *s, size_t init)
{
    s->ptrs = malloc(sizeof(pipos_t) * init);
    if (s->ptrs == NULL)
        return ENOMEM;
    s->size = init;
    s->fill = 0;
    return 0;
}

static void pstack_free(struct pstack *s)
{
    free(s->ptrs);
    s->ptrs = NULL;
}

static int pstack_grow(struct pstack *s)
{
    s->size *= 2;
    s->ptrs = realloc(s->ptrs, s->size * sizeof(pipos_t));
    if (s->ptrs == NULL)
        return ENOMEM;
    return 0;
}

static int pstack_push(struct pstack *s, pipos_t ptr)
{
    if (s->fill == s->size)
        if (pstack_grow(s) != 0)
            return ENOMEM;
    s->ptrs[s->fill] = ptr;
    s->fill++;
    return 0;
}

static inline uint64_t pow10(int p)
{
    int x = 1;
    for (int i = 0; i < p; i++, x *= 10);
    return x;
}

static inline void report(pipos_t n)
{
    fprintf(stderr, "%" PRpipos " digits loaded ...\n", n);
}

int offsetdb_create(const char *dbfile, const char *pifile, int psize)
{
    FILE *pi = fopen(pifile, "rb");
    if (pi == NULL)
        return errno;
    FILE *out = fopen(dbfile, "wb");
    if (out == NULL)
        return errno;
    uint64_t psize_out = psize;
    if (fwrite(&psize_out, sizeof(psize_out), 1, out) != 1)
        return errno;

    /* Compute sizes */
    size_t nchunks = pow10(psize);
    struct pstack *stacks = malloc(nchunks * sizeof(*stacks));
    for (size_t c = 0; c < nchunks; c++)
        if (pstack_init(stacks + c, 8) != 0)
            return ENOMEM;

    /* Parse and index digits */
    fprintf(stderr, "Loading digits ...\n");
    char buffer[psize + 1];
    buffer[psize] = '\0';
    if (fread(buffer, psize, 1, pi) != 1)
        return errno;
    pipos_t pos = 0;
    for (int c = fgetc(pi); c != EOF; c = fgetc(pi), pos++) {
        if (pos % 1000000 == 0)
            report(pos);
        memmove(buffer, buffer + 1, psize - 1);
        buffer[psize - 1] = c;
        int pattern = strtol(buffer, NULL, 10);
        if (pstack_push(stacks + pattern, pos) != 0)
            return ENOMEM;
    }
    report(pos);
    fclose(pi);

    /* Write index */
    fprintf(stderr, "Writing index ...\n");
    uint64_t sum = (2 + nchunks) * sizeof(uint64_t);
    for (size_t c = 0; c < nchunks; c++) {
        if (fwrite(&sum, sizeof(sum), 1, out) != 1)
            return errno;
        sum += stacks[c].fill * sizeof(pipos_t);
    }
    if (fwrite(&sum, sizeof(sum), 1, out) != 1)
        return errno;

    /* Write pointers */
    fprintf(stderr, "Writing tables ...\n");
    for (size_t c = 0; c < nchunks; c++) {
        size_t size = stacks[c].fill;
        if (fwrite(stacks[c].ptrs, sizeof(pipos_t), size, out) != size)
            return errno;
        pstack_free(stacks + c);
    }

    free(stacks);
    fprintf(stderr, "Done.\n");
    return fclose(out);
}

int offsetdb_open(offsetdb_t *db, const char *file, const char *digits)
{
    if ((db->db = fopen(file, "rb")) == NULL)
        return errno;
    if ((db->pi = fopen(digits, "rb")) == NULL) {
        fclose(db->db);
        return errno;
    }
    uint64_t psize;
    if (fread(&psize, sizeof(psize), 1, db->db) != 1)
        return errno;
    db->psize = psize;
    return 0;
}

void offsetdb_close(offsetdb_t *db)
{
    fclose(db->db);
}

/**
 * Returns the start and end file positions for the segment of the
 * database containing the pi offsets for a pattern. If this function
 * returns 1, positions need to be checked for matches (the pattern
 * exceeds the database psize). In case of error, returns -1;
 */
static int
bounds(offsetdb_t *db, const char *p, uint64_t *start, uint64_t *end)
{
    size_t length = strlen(p);
    uint64_t range = pow10(db->psize - length);
    uint64_t value;
    if (length <= db->psize) {
        value = strtol(p, NULL, 10);
    } else {
        char copy[db->psize + 1];
        memcpy(copy, p, db->psize);
        copy[db->psize] = '\0';
        value = strtol(copy, NULL, 10);
    }
    uint64_t pstart = value * range;
    uint64_t pend = pstart + range - 1;
    long ostart = (1 + pstart) * sizeof(uint64_t);
    long oend = (1 + pend) * sizeof(uint64_t);
    if (fseek(db->db, ostart, SEEK_SET) != 0)
        return -1;
    if (fread(start, sizeof(*start), 1, db->db) != 1)
        return -1;
    if (ostart != oend)
        if (fseek(db->db, oend + sizeof(uint64_t), SEEK_SET) != 0)
            return -1;
    if (fread(end, sizeof(*end), 1, db->db) != 1)
        return -1;
    return length > db->psize;
}

int offsetdb_begin(offsetdb_t *db, const char *pattern)
{
    db->pattern = pattern;
    db->check = bounds(db, pattern, &db->position, &db->stop);
    if (db->check < 0)
        return -1;
    if (fseek(db->db, db->position, SEEK_SET) != 0)
        return errno;
    return 0;
}

int offsetdb_step(offsetdb_t *db, pipos_t *pos, char *context, size_t size)
{
    size_t plength = strlen(db->pattern);
    size_t length = max(plength + 1, size);
    char digits[length];
    memset(digits, 0, length);
    pipos_t out = 0;
    while (db->position < db->stop) {
        if (fread(&out, sizeof(out), 1, db->db) != 1)
            return errno;
        db->position += sizeof(pipos_t);
        if (db->check || context != NULL) {
            if (fseek(db->pi, out + 1, SEEK_SET) != 0)
                return errno;
            if (fread(digits, length - 1, 1, db->pi) != 1)
                return errno;
            if (!db->check)
                break;
            if (memcmp(db->pattern, digits, plength) == 0)
                break;
        }
        out = 0;
    }
    *pos = out;
    if (context != NULL)
        snprintf(context, size, "%s", digits);
    return 0;
}

size_t offsetdb_nresults(offsetdb_t *db)
{
    return (db->stop - db->position) / sizeof(pipos_t);
}

int offsetdb_print(offsetdb_t *db, const char *pattern)
{
    if (offsetdb_begin(db, pattern) != 0)
        return -1;
    size_t context_size = max(db->psize, strlen(pattern)) * 2;
    char context[context_size];
    while (offsetdb_nresults(db) > 0) {
        pipos_t pos;
        if (offsetdb_step(db, &pos, context, context_size) != 0)
            return -1;
        if (pos)
            printf("%" PRpipos ": %s\n", pos, context);
    }
    return 0;
}
