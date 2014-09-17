#ifndef OFFSETDB_H
#define OFFSETDB_H

#include <stdio.h>
#include <stdint.h>

typedef uint32_t pipos_t;
#define PRpipos "u"
#define PIPOS_END  0
#define PIPOS_MORE UINTMAX_C(pipos_t)

typedef struct offsetdb {
    FILE *db, *pi;
    int psize;
    int check;
    uint64_t position, stop;
    const char *pattern;
} offsetdb_t;

/**
 * Creates a new pi search database.
 * @dbfile:  Output file for the database.
 * @pifile:  Input file containing pi digits.
 * @psize:   Pattern size to index on (best around 6 - 8).
 *
 * This function requires lots of memory. For a 1 billion digit file,
 * this will need about 4GB of RAM.
 *
 * Return: An errno if an error occurs.
 */
int offsetdb_create(const char *dbfile, const char *pifile, int psize);

/**
 * Opens a database for searching.
 * @db:      Database connection object.
 * @index:   Database file.
 * @digits:  Original pi digits file.
 *
 * Return: An errno if an error occurs.
 */
int offsetdb_open(offsetdb_t *db, const char *index, const char *digits);

/**
 * Closes a database connection freeing all resources.
 * @db:  Database connection object.
 */
void offsetdb_close(offsetdb_t *db);

/**
 * Initializes a pattern search.
 * @db:       Database connection object.
 * @pattern:  Pattern to search for.
 *
 * The pattern pointer must remain valid and constant through the
 * entire search. A local copy is not made.
 *
 * Return: An errno if an error occurs.
 */
int offsetdb_begin(offsetdb_t *db, const char *pattern);

/**
 * Retrieves the next digit position for the set pattern.
 * @db:       Database connection object.
 * @pos:      Output parameter for digit position.
 *            A position of 0 indicates the end of the search.
 * @context:  Buffer for putting context digits. Can be NULL.
 * @size:     Size of the context buffer.
 */
int offsetdb_step(offsetdb_t *db, pipos_t *pos, char *context, size_t size);

/**
 * Get the maximum number of search results.
 * @db:  Database connection object.
 *
 * Return: The maximum number of search results; actual count may be fewer.
 */
size_t offsetdb_nresults(offsetdb_t *db);

/**
 * Convenience function to print all search results.
 * @db:       Database connection object.
 * @pattern:  Pattern to be searched for.
 *
 * Return: An errno if an error occurs.
 */
int offsetdb_print(offsetdb_t *db, const char *pattern);

#endif
