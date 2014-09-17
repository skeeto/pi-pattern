#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include "offsetdb.h"

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
            fprintf(stderr, "%" PRpipos " digits loaded ...\n", pos);
        memmove(buffer, buffer + 1, psize - 1);
        buffer[psize - 1] = c;
        int pattern = strtol(buffer, NULL, 10);
        if (pstack_push(stacks + pattern, pos) != 0)
            return ENOMEM;
    }
    fclose(pi);

    /* Write index */
    uint64_t sum = (2 + nchunks) * sizeof(uint64_t);
    for (size_t c = 0; c < nchunks; c++) {
        if (fwrite(&sum, sizeof(sum), 1, out) != 1)
            return errno;
        sum += stacks[c].fill * sizeof(pipos_t);
    }
    if (fwrite(&sum, sizeof(sum), 1, out) != 1)
        return errno;

    /* Write pointers */
    for (size_t c = 0; c < nchunks; c++) {
        size_t size = stacks[c].fill;
        if (fwrite(stacks[c].ptrs, sizeof(pipos_t), size, out) != size)
            return errno;
        pstack_free(stacks + c);
    }

    free(stacks);
    return fclose(out);
}

int offsetdb_open(struct offsetdb *db, const char *file, const char *digits)
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

void offsetdb_close(struct offsetdb *db)
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
bounds(struct offsetdb *db, const char *p, uint64_t *start, uint64_t *end)
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

int offsetdb_search(struct offsetdb *db, const char *pattern)
{
    size_t length = strlen(pattern);
    uint64_t chunk_start, chunk_end;
    int check = bounds(db, pattern, &chunk_start, &chunk_end);
    if (check < 0)
        return -1;
    if (fseek(db->db, chunk_start, SEEK_SET) != 0)
        return errno;
    size_t context_size = length * 2 < 8 ? 8 : length * 2;
    for (uint64_t i = chunk_start; i < chunk_end; i += sizeof(pipos_t)) {
        pipos_t pos;
        if (fread(&pos, sizeof(pos), 1, db->db) != 1)
            return errno;
        char context[context_size + 1];
        if (fseek(db->pi, pos + 1, SEEK_SET) != 0)
            return errno;
        if (fread(&context, 1, context_size, db->pi) != context_size)
            return errno;
        context[context_size] = '\0';
        if (!check || memcmp(pattern, context, length) == 0)
            printf("%" PRpipos ": %s\n", pos, context);
    }
    return 0;
}
