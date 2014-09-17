#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <locale.h>
#include <string.h>
#include <errno.h>
#include "flat.h"
#include "offsetdb.h"

enum mode { FLAT, OFFSET };

void print_usage(const char *name, int ret)
{
    printf("Usage: %s [OPTION] [PATTERN]\n", name);
    printf("  -i <digits file>     UTF-8 pi digit listing (pi-billion.txt)\n");
    printf("  -d <database file>   Database filename\n");
    printf("  -f                   Do a straight string search (no index)\n");
    printf("  -b                   Do an offset index search\n");
    printf("  -h                   Print this message and exit\n");
    exit(ret);
}

int main(int argc, char **argv)
{
    setlocale(LC_NUMERIC, "");

    /* Options */
    enum mode mode = OFFSET;
    const char *datafile = "pi-billion.txt";
    const char *indexfile = NULL;

    /* Parse options */
    int opt;
    while ((opt = getopt(argc, argv, "d:i:bhf")) != -1) {
        switch (opt) {
        case 'i':
            datafile = optarg;
            break;
        case 'd':
            indexfile = optarg;
            break;
        case 'f':
            mode = FLAT;
            break;
        case 'b':
            mode = OFFSET;
            break;
        case 'h':
            print_usage(argv[0], EXIT_SUCCESS);
            break;
        default:
            print_usage(argv[0], EXIT_FAILURE);
        }
    }

    if (mode == OFFSET) {
        if (indexfile == NULL)
            indexfile = "pi.offsetdb";
        FILE *test;
        if ((test = fopen(indexfile, "r")) == NULL) {
            if (offsetdb_create(indexfile, datafile, 7) != 0) {
                fprintf(stderr, "error: index failed, %s\n", strerror(errno));
                remove(indexfile);
                exit(EXIT_FAILURE);
            }
        } else {
            fclose(test);
        }
        struct offsetdb db;
        if (offsetdb_open(&db, indexfile, datafile) != 0) {
            fprintf(stderr, "error: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        for (int i = optind; i < argc; i++) {
            if (offsetdb_print(&db, argv[i]) != 0) {
                fprintf(stderr, "error: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        offsetdb_close(&db);
    } else if (mode == FLAT) {
        for (int i = optind; i < argc; i++)
            flat_search(datafile, argv[i]);
    }
    return 0;
}
