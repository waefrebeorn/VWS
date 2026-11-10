/*
 * vws_templates_test.c -- the template library's gate.
 *
 * A template is only useful if it is a real program: every template must emit
 * a complete .wet script, that script must load back through the designer's
 * own loader, and the loaded form must match the template's data.  The round
 * trip is the .frm guarantee applied to templates.
 */
#include "vws.h"
#include "vws_templates.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_fail;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", (msg)); g_fail++; } \
} while (0)

static bool has(const char *hay, const char *needle) {
    return hay && strstr(hay, needle) != NULL;
}

/* Write a script to a temp file so the real loader can read it. */
static char *write_temp(const char *script) {
    char path[] = "/tmp/vws_tpl_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return NULL;
    size_t len = strlen(script);
    if (write(fd, script, len) != (ssize_t)len) { close(fd); unlink(path); return NULL; }
    close(fd);
    return strdup(path);
}

static int form_count_ids(VwsForm *f, char ids[][64], int cap) {
    int n = 0;
    for (VwsControl *c = f->controls; c && n < cap; c = c->next) {
        snprintf(ids[n], 64, "%s", c->id);
        n++;
    }
    return n;
}

int main(void) {
    int n = vws_template_count();
    CHECK(n >= 8, "at least eight templates are registered");
    printf("templates: %d\n", n);

    /* Unknown names must fail cleanly, not crash. */
    CHECK(vws_template_find("nope") == NULL, "unknown template -> NULL");
    CHECK(vws_template_emit("nope") == NULL, "unknown template emits NULL");
    CHECK(vws_template_emit(NULL) == NULL, "NULL template emits NULL");

    for (int i = 0; i < n; i++) {
        const VwsTemplate *t = vws_template_at(i);
        CHECK(t != NULL, "template_at in range");
        if (!t) continue;
        printf("  %-12s %s\n", t->name, t->label);

        /* 1. Emit a complete, runnable script. */
        char *script = vws_template_emit(t->name);
        CHECK(script != NULL, "template emits");
        if (!script) continue;
        CHECK(has(script, "wet.applet {"), "script has an applet");
        CHECK(has(script, "content = wet.canvas"), "script has a canvas");
        CHECK(strlen(script) > 40, "script is not empty");
        free(script);

        /* 2. The script loads back through the real designer loader. */
        script = vws_template_emit(t->name);
        char *path = write_temp(script);
        CHECK(path != NULL, "temp file written");
        if (path) {
            char err[256] = {0};
            VwsForm *f = vws_form_load(path, err, sizeof err);
            CHECK(f != NULL, "template script loads");
            if (f) {
                CHECK(f->width  == t->def_w,  "loaded width matches template");
                CHECK(f->height == t->def_h,  "loaded height matches template");
                CHECK(f->count  > 0 || t->def_w == 320, "loaded form has controls");

                /* 3. A form built from the template data matches the load. */
                VwsForm *g = vws_form_new("x", 1, 1);
                CHECK(g != NULL, "form_new");
                if (g) {
                    CHECK(vws_form_load_template(t->name, g), "load_template");
                    CHECK(g->count == f->count, "template form count == loaded count");
                    CHECK(g->width  == f->width,  "template form width == loaded");
                    CHECK(g->height == f->height, "template form height == loaded");
                    char a[64][64], b[64][64];
                    int na = form_count_ids(f, a, 64);
                    int nb = form_count_ids(g, b, 64);
                    CHECK(na == nb, "same control count");
                    bool same = na == nb;
                    for (int k = 0; same && k < na; k++)
                        if (strcmp(a[k], b[k]) != 0) same = false;
                    CHECK(same, "same control ids in the same order");
                    vws_form_free(g);
                }
                vws_form_free(f);
            }
            unlink(path);
            free(path);
        }
        free(script);
    }

    /* 4. load_template on a fresh form, then emit -> the handler wiring shows. */
    VwsForm *f = vws_form_new("x", 1, 1);
    CHECK(f != NULL, "form_new");
    if (f) {
        CHECK(vws_form_load_template("dialog", f), "dialog loads");
        CHECK(f->count == 4, "dialog has four controls");
        VwsControl *ok = vws_form_find(f, "OK");
        CHECK(ok != NULL, "OK control exists");
        if (ok) {
            CHECK(strcmp(ok->handler, "OK_Click") == 0, "OK keeps its handler name");
            char *script = vws_form_emit(f);
            CHECK(script != NULL, "emit after load_template");
            if (script) {
                CHECK(has(script, "onclick=OK_Click"), "handler name reaches the script");
                free(script);
            }
        }
        vws_form_free(f);
    }

    if (g_fail == 0) printf("VWS TEMPLATES: ALL PASS\n");
    else            printf("VWS TEMPLATES: %d FAIL\n", g_fail);
    return g_fail ? 1 : 0;
}
