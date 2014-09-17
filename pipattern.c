#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <locale.h>
#include <string.h>
#include <errno.h>
#include "flat.h"
#include "offsetdb.h"
#include "sqlite_index.h"

enum mode { FLAT, OFFSET, SQLITE };

void print_usage(const char *name, int ret)
{
    printf("Usage: %s [OPTION] [PATTERN]\n", name);
    printf("  -i <digits file>     UTF-8 pi digit listing (pi-billion.txt)\n");
    printf("  -d <database file>   Database filename\n");
    printf("  -n <length>          Maximum pattern length (larger index)\n");
    printf("  -f                   Do a straight string search (no index)\n");
    printf("  -b                   Do an offset index search\n");
    printf("  -s                   Do a SQLite index search\n");
    printf("  -q                   Don't print indexing progress\n");
    printf("  -h                   Print this message and exit\n");
    exit(ret);
}

int main(int argc, char **argv)
{
    setlocale(LC_NUMERIC, "");

    /* Options */
    bool quiet = false;
    enum mode mode = OFFSET;
    int max_length = 16;
    const char *datafile = "pi-billion.txt";
    const char *indexfile = NULL;

    /* Parse options */
    int opt;
    while ((opt = getopt(argc, argv, "d:i:n:bsqhf")) != -1) {
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
            mode = FLAT;
            break;
        case 'b':
            mode = OFFSET;
            break;
        case 's':
            mode = SQLITE;
            break;
        case 'h':
            print_usage(argv[0], EXIT_SUCCESS);
            break;
        default:
            print_usage(argv[0], EXIT_FAILURE);
        }
    }

    if (mode == SQLITE) {
        struct index index;
        if (indexfile == NULL)
            indexfile = "pi.sqlite";
        index_open(&index, indexfile);
        index.quiet = quiet;
        if (index_count(&index) == 0)
            index_load(&index, datafile, max_length);
        for (int i = optind; i < argc; i++)
            index_search(&index, argv[i]);
        index_close(&index);
    } else if (mode == OFFSET) {
        if (indexfile == NULL)
            indexfile = "pi.offsetdb";
        FILE *test;
        if ((test = fopen(indexfile, "r")) == NULL) {
            if (offsetdb_create(indexfile, datafile, 6) != 0) {
                fprintf(stderr, "error: index failed, %s\n", strerror(errno));
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
