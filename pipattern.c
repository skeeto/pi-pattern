#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <locale.h>
#include "flat.h"
#include "sqlite_index.h"

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
    setlocale(LC_NUMERIC, "");

    /* Options */
    bool quiet = false;
    bool use_index = true;
    int max_length = 16;
    const char *datafile = "pi-billion.txt";
    const char *indexfile = "pi.sqlite";

    /* Parse options */
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
        index.quiet = quiet;
        if (index_count(&index) == 0)
            index_load(&index, datafile, max_length);
        for (int i = optind; i < argc; i++)
            index_search(&index, argv[i]);
        index_close(&index);
    } else {
        for (int i = optind; i < argc; i++)
            flat_search(datafile, argv[i]);
    }
    return 0;
}
