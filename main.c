#include "libqbe.h"
#include "all.h"
#include "config.h"
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Target T;

int
main(int ac, char *av[])
{
    QBE *q;
    char *f;
    int c, dbg = 0;

    q = qbe_new();
    if (!q) {
        fprintf(stderr, "failed to create QBE context\n");
        exit(1);
    }
    qbe_init_defaults(q);

    while ((c = getopt(ac, av, "hd:o:t:")) != -1)
        switch (c) {
        case 'd':
            qbe_set_debug_flags(q, optarg);
            dbg = 1;
            break;
        case 'o':
            if (qbe_set_output_file(q, optarg) != 0) {
                fprintf(stderr, "%s\n", qbe_get_error(q));
                qbe_free(q);
                exit(1);
            }
            break;
        case 't':
            if (strcmp(optarg, "?") == 0) {
                puts(T.name);
                qbe_free(q);
                exit(0);
            }
            if (qbe_set_target(q, optarg) != 0) {
                fprintf(stderr, "%s\n", qbe_get_error(q));
                qbe_free(q);
                exit(1);
            }
            break;
        case 'h':
        default: {
            FILE *hf = (c != 'h') ? stderr : stdout;
            int i, count;
            char *sep;
            
            fprintf(hf, "%s [OPTIONS] {file.ssa, -}\n", av[0]);
            fprintf(hf, "\t%-11s prints this help\n", "-h");
            fprintf(hf, "\t%-11s output to file\n", "-o file");
            fprintf(hf, "\t%-11s generate for a target among:\n", "-t <target>");
            fprintf(hf, "\t%-11s ", "");
            
            count = qbe_target_count();
            for (i = 0, sep = ""; i < count; i++, sep = ", ") {
                const char *name = qbe_target_name(i);
                fprintf(hf, "%s%s", sep, name);
                if (i == 0)
                    fputs(" (default)", hf);
            }
            fprintf(hf, "\n");
            fprintf(hf, "\t%-11s dump debug information\n", "-d <flags>");
            
            qbe_free(q);
            exit(c != 'h');
        }
        }

    do {
        f = av[optind];
        if (!f || strcmp(f, "-") == 0) {
            if (qbe_compile_stream(q, stdin, "-") != 0) {
                fprintf(stderr, "%s\n", qbe_get_error(q));
                qbe_free(q);
                exit(1);
            }
        } else {
            if (qbe_compile_file(q, f) != 0) {
                fprintf(stderr, "%s\n", qbe_get_error(q));
                qbe_free(q);
                exit(1);
            }
        }
    } while (++optind < ac);

    if (!dbg)
        qbe_finish(q);

    qbe_free(q);
    exit(0);
}