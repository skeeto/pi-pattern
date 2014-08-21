#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include "flat.h"

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
