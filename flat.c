#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include "flat.h"

#define CONTEXT 8

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
    size_t hfill, hlength = length * 16;
    unsigned char haystack[hlength];
    if ((hfill = fread(haystack, 1, hlength, pi)) == 0 && ferror(pi)) {
        perror(file);
        exit(EXIT_FAILURE);
    }

    /* Perform search on stream */
    size_t position = 0;
    while (hfill >= length) {
        for (size_t i = last; haystack[i] == pattern[i]; i--) {
            if (i == 0) {
                char context[length + CONTEXT + 1];
                memcpy(context, haystack, length + CONTEXT);
                context[length + CONTEXT] = '\0';
                printf("%" PRIu64 ": %s\n", position - 1, context);
            }
        }
        size_t skip = bad_char_skip[haystack[last]];
        position += skip;
        hfill -= skip;
        memmove(haystack, haystack + skip, hlength - skip);
        if (hfill < length + CONTEXT) {
            size_t in = fread(haystack + hfill, 1, hlength - hfill, pi);
            if (in == 0 && ferror(pi)) {
                perror(file);
                exit(EXIT_FAILURE);
            }
            hfill += in;
        }
    }

    if (fclose(pi) != 0) {
        perror(file);
        exit(EXIT_FAILURE);
    }
}
