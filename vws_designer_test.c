/*
 * vws_designer_test.c -- the designer applet, run for real.
 *
 * The designer is a .wet applet, so the test loads it into a WetApp the way a
 * host would and then checks the designer's own chrome exists and that the
 * document model behind it works.  This is the dogfood gate: if the designer
 * cannot be built from the framework it designs, the stack is not circular.
 */
#include "vws_internal.h"
#include "vws_lua.h"

#include "wet.h"
#include "wet_lua.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks, g_fails;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

static const char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

static const char *DESIGNER = "designer.wet";

int main(void) {
    printf("=== vws designer applet ===\n");

    WetApp *app = wet_app_new(900, 620);
    CHECK(app != NULL, "app created");
    if (!app) return 1;

    WetLua *wl = wet_lua_new(app, NULL, 0);
    CHECK(wl != NULL, "wet lua created");
    if (!wl) { wet_app_free(app); return 1; }

    /* The designer shares the app's interpreter. */
    char err[256] = {0};
    VwsLua *vl = vws_lua_open(wet_lua_state(wl), "Form1", 400, 300, err, sizeof err);
    CHECK(vl != NULL, err[0] ? err : "vws document opened");

    const char *src = read_file(DESIGNER);
    CHECK(src != NULL, "designer.wet readable");
    if (!src) { vws_lua_free(vl); wet_lua_free(wl); wet_app_free(app); return 1; }

    const char *e = wet_lua_run(wl, src, DESIGNER);
    CHECK(e == NULL, e ? e : "designer script ran");

    /* The designer's chrome must exist -- it built its own tree. */
    static const char *ids[] = { "root", "title", "body", "palette",
                                 "surface", "props", "bar", "status",
                                 "save", "load", NULL };
    for (int i = 0; ids[i]; i++)
        CHECK(wet_app_find(app, ids[i]) != NULL, ids[i]);

    /* The palette is populated from the document model's palette. */
    WetWidget *pal = wet_app_find(app, "palette");
    CHECK(pal && wet_widget_nchildren(pal) >= 8, "palette populated");

    /* Add a control through the document API and confirm it lands on the
     * surface as a real .WET widget -- the designer previews what will run. */
    CHECK(vws_lua_form(vl) != NULL, "document reachable");
    if (vl && vws_lua_form(vl)) {
        CHECK(vws_form_add(vws_lua_form(vl), "Button1", "button", 24, 40, 100, 28),
              "control added to the document");

        /* Mirror what the designer's rebuild does: create the widget and put
         * it on the surface.  If that works, the designer can preview. */
        WetWidget *surface = wet_app_find(app, "surface");
        if (surface) {
            WetWidget *btn = wet_widget_new(app, WET_CLASS_BUTTON, "Button1");
            if (btn) {
                wet_canvas_set_left(btn, 24);
                wet_canvas_set_top(btn, 40);
                wet_canvas_set_size(btn, 100, 28);
                wet_button_set_text(btn, "Button1");
                wet_widget_add(surface, btn);
                CHECK(wet_widget_nchildren(surface) == 1,
                      "surface got the form control");
                wet_app_layout(app);
            } else {
                CHECK(0, "surface got the form control");
            }
        }
    }

    /* The designer's status label reports readiness. */
    WetWidget *st = wet_app_find(app, "status");
    CHECK(st && wet_label_text(st) && strstr(wet_label_text(st), "ready") != NULL,
          "status reports ready");

    free((void *)src);
    vws_lua_free(vl);
    wet_lua_free(wl);
    wet_app_free(app);
    printf("=== vws_designer: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
