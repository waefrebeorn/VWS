/*
 * vws_load.c -- turn a .vws file back into a VwsForm.
 *
 * A .vws file is a Lua script, so loading is: read it, run it through .WET,
 * then walk the built widget tree.  The designer's view and the running app
 * therefore agree by construction -- there is no separate design format to
 * deserialise and no chance of the two drifting.
 *
 * Handler bodies come from the SOURCE, not the tree: the tree only knows that a
 * control has a handler function, not its text, and re-emitting from the tree
 * alone would silently delete the user's code.
 */
#include "vws_internal.h"

#include "wet.h"
#include "wet_lua.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The caption a control carries, by class. */
static void read_text(WetWidget *w, char *out, size_t cap) {
    out[0] = '\0';
    if (!w) return;
    const char *s = NULL;
    switch (wet_widget_class(w)) {
    case WET_CLASS_LABEL:    s = wet_label_text(w);    break;
    case WET_CLASS_BUTTON:   s = wet_button_text(w);   break;
    case WET_CLASS_TEXTBOX:  s = wet_textbox_text(w);  break;
    case WET_CLASS_CHECKBOX: s = wet_checkbox_text(w); break;
    case WET_CLASS_GROUPBOX: s = wet_groupbox_text(w); break;
    default: break;
    }
    if (s) snprintf(out, cap, "%s", s);
}

/* Walk the applet's root into the form.  A .vws form's content is a
 * wet.canvas, so its children carry absolute left/top -- VB's model. */
static void walk(WetApp *app, VwsForm *f) {
    WetWidget *root = wet_app_root_get(app);
    if (!root) return;

    f->width  = wet_app_width(app);
    f->height = wet_app_height(app);

    /* A canvas root's children are the controls.  A bare control root is a
     * one-control form.  Anything else is still walkable: the controls are the
     * root's descendants at depth 1. */
    WetWidget *parent = (wet_widget_nchildren(root) > 0) ? root : NULL;
    int n = parent ? wet_widget_nchildren(parent) : 1;

    for (int i = 0; i < n; i++) {
        WetWidget *c = parent ? wet_widget_child(parent, i) : root;
        if (!c) continue;

        const char *id = wet_widget_id(c);
        if (!id || !id[0]) continue;      /* unnamed controls are not designable */

        WetRect r = wet_widget_rect(c);
        VwsControl *vc = vws_form_add(f, id, wet_widget_class_name(c),
                                      wet_canvas_left(c), wet_canvas_top(c),
                                      r.w, r.h);
        if (!vc) continue;
        read_text(c, vc->text, sizeof vc->text);
        /* The handler NAME is a design-time field; the tree holds the function
         * value, so a loaded form leaves it empty and the designer sets it. */
        vc->handler[0] = '\0';
    }
}

/* Read a whole file.  Returns NULL on failure. */
static char *read_file(const char *path, long *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    if (out_len) *out_len = (long)got;
    return buf;
}

VwsForm *vws_load_from_app(void *wet_app, VwsSource *src) {
    if (!wet_app) return NULL;
    WetApp *app = (WetApp *)wet_app;

    /* Recover the title from the script's wet.applet{...} if it declared one;
     * the form's own size comes from the app. */
    VwsForm *f = vws_form_new("Form1", wet_app_width(app), wet_app_height(app));
    if (!f) return NULL;

    walk(app, f);

    if (src && src->src) {
        if (vws_scan_handlers(src, f) < 0) {
            vws_form_set_error("unterminated function body in source");
            vws_form_free(f);
            return NULL;
        }
    }
    return f;
}

VwsForm *vws_form_load(const char *path, char *err, size_t err_cap) {
    vws_form_set_error(NULL);

    long len = 0;
    char *src = read_file(path, &len);
    if (!src) {
        char buf[320];
        snprintf(buf, sizeof buf, "vws: cannot read %s", path ? path : "(null)");
        if (err) snprintf(err, err_cap, "%s", buf);
        vws_form_set_error(buf);
        return NULL;
    }

    WetApp *app = wet_app_new(400, 300);
    if (!app) { free(src); vws_form_set_error("vws: app alloc failed"); return NULL; }

    WetLua *wl = wet_lua_new(app, err, err_cap);
    if (!wl) { wet_app_free(app); free(src); return NULL; }

    const char *e = wet_app_load(app, src, path);
    if (e) {
        char buf[320];
        snprintf(buf, sizeof buf, "vws: %s", e);
        if (err) snprintf(err, err_cap, "%s", buf);
        vws_form_set_error(buf);
        wet_lua_free(wl);
        wet_app_free(app);
        free(src);
        return NULL;
    }

    VwsSource vs;
    vs.src = src;
    vs.len = len;
    VwsForm *f = vws_load_from_app(app, &vs);

    wet_lua_free(wl);
    wet_app_free(app);
    free(src);
    return f;
}
