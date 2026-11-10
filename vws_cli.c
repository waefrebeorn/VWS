/*
 * vws_cli.c -- the VWS command line.
 *
 * The designer's template library is reachable without opening a window, which
 * is what lets the IDE (W-CODE) create a program from a template by running one
 * command -- the same fork/exec shape its build op already uses.  The template
 * text comes from vws_templates.c, so the CLI and the designer can never
 * disagree about what a template contains.
 *
 *   vws --list                     picker table: name, label, size
 *   vws --template <name>          the complete runnable .wet script on stdout
 *   vws --new <name> <out.vws>     write that script to a file
 *   vws --help
 *
 * Exit codes: 0 ok, 1 usage, 2 unknown template, 3 could not write.
 */
#include "vws_templates.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr,
        "vws -- Visual WuBu System (WuBu's Visual Basic)\n"
        "\n"
        "  vws --list                   list the templates\n"
        "  vws --template <name>        print the template as a runnable .wet script\n"
        "  vws --new <name> <out.vws>   write the template to a file\n"
        "  vws --help\n"
        "\n"
        "A .vws file is a .wet Lua script: what the CLI prints is exactly what\n"
        "the designer saves, and it runs as-is.\n");
    return 1;
}

static int list_templates(void) {
    int n = vws_template_count();
    printf("%-12s %-20s %-8s %s\n", "NAME", "LABEL", "SIZE", "DESCRIPTION");
    for (int i = 0; i < n; i++) {
        const VwsTemplate *t = vws_template_at(i);
        if (!t) continue;
        printf("%-12s %-20s %dx%-4d  %s\n",
               t->name, t->label, t->def_w, t->def_h, t->description);
    }
    printf("\n%d templates\n", n);
    return 0;
}

static char *emit_or_null(const char *name) {
    return vws_template_emit(name);
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();

    if (strcmp(argv[1], "--list") == 0)
        return list_templates();

    if (strcmp(argv[1], "--template") == 0) {
        if (argc < 3) return usage();
        char *script = emit_or_null(argv[2]);
        if (!script) {
            fprintf(stderr, "vws: unknown template '%s' (try --list)\n", argv[2]);
            return 2;
        }
        fputs(script, stdout);
        free(script);
        return 0;
    }

    if (strcmp(argv[1], "--new") == 0) {
        if (argc < 4) return usage();
        char *script = emit_or_null(argv[2]);
        if (!script) {
            fprintf(stderr, "vws: unknown template '%s' (try --list)\n", argv[2]);
            return 2;
        }
        FILE *fp = fopen(argv[3], "wb");
        if (!fp) {
            fprintf(stderr, "vws: cannot write '%s'\n", argv[3]);
            free(script);
            return 3;
        }
        size_t len = strlen(script);
        size_t wrote = fwrite(script, 1, len, fp);
        fclose(fp);
        free(script);
        if (wrote != len) {
            fprintf(stderr, "vws: short write to '%s'\n", argv[3]);
            return 3;
        }
        printf("vws: wrote %s from template '%s'\n", argv[3], argv[2]);
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage();
        return 0;
    }

    fprintf(stderr, "vws: unknown option '%s'\n", argv[1]);
    return usage();
}
