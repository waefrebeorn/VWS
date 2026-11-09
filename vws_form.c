/*
 * vws_form.c -- the designer's document: the form, its controls, and the
 * edits a designer surface makes.  Nothing here knows about Lua or .WET; the
 * emitter and the loader are separate concerns.
 */
#include "vws_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ form */

VwsForm *vws_form_new(const char *title, int width, int height) {
    VwsForm *f = (VwsForm *)calloc(1, sizeof *f);
    if (!f) return NULL;
    snprintf(f->title, sizeof f->title, "%s", title ? title : "Form1");
    f->width  = width  > 0 ? width  : 400;
    f->height = height > 0 ? height : 300;
    return f;
}

static void free_handlers(VwsForm *f) {
    for (int i = 0; i < f->nhandlers; i++) free(f->handlers[i].body);
    free(f->handlers);
    f->handlers = NULL;
    f->nhandlers = 0;
}

void vws_form_free(VwsForm *f) {
    if (!f) return;
    VwsControl *c = f->controls;
    while (c) { VwsControl *n = c->next; free(c); c = n; }
    free_handlers(f);
    free(f);
}

/* --------------------------------------------------------------- controls */

VwsControl *vws_form_add(VwsForm *f, const char *id, const char *cls,
                         int left, int top, int w, int h) {
    if (!f || !id || !cls) return NULL;
    /* Ids are how the script reaches a control, so they must be unique. */
    if (vws_form_find(f, id)) return NULL;

    VwsControl *c = (VwsControl *)calloc(1, sizeof *c);
    if (!c) return NULL;
    snprintf(c->id,   sizeof c->id,   "%s", id);
    snprintf(c->cls,  sizeof c->cls,  "%s", cls);
    c->left = left; c->top = top;
    c->w = w > 0 ? w : 80;
    c->h = h > 0 ? h : 24;

    /* Append: the last control is the topmost, which is how a designer paints. */
    if (!f->controls) {
        f->controls = c;
    } else {
        VwsControl *p = f->controls;
        while (p->next) p = p->next;
        p->next = c;
    }
    f->count++;
    return c;
}

VwsControl *vws_form_find(VwsForm *f, const char *id) {
    if (!f || !id) return NULL;
    for (VwsControl *c = f->controls; c; c = c->next)
        if (strcmp(c->id, id) == 0) return c;
    return NULL;
}

/* Topmost control whose rect contains the point.  Controls are in z-order,
 * bottom first, so the last match wins. */
VwsControl *vws_form_at(VwsForm *f, int x, int y) {
    if (!f) return NULL;
    VwsControl *hit = NULL;
    for (VwsControl *c = f->controls; c; c = c->next) {
        if (x >= c->left && x < c->left + c->w &&
            y >= c->top  && y < c->top  + c->h) hit = c;
    }
    return hit;
}

bool vws_form_move(VwsForm *f, const char *id, int left, int top) {
    VwsControl *c = vws_form_find(f, id);
    if (!c) return false;
    c->left = left; c->top = top;
    return true;
}

bool vws_form_resize(VwsForm *f, const char *id, int w, int h) {
    VwsControl *c = vws_form_find(f, id);
    if (!c) return false;
    if (w > 0) c->w = w;
    if (h > 0) c->h = h;
    return true;
}

bool vws_form_set_text(VwsForm *f, const char *id, const char *text) {
    VwsControl *c = vws_form_find(f, id);
    if (!c) return false;
    snprintf(c->text, sizeof c->text, "%s", text ? text : "");
    return true;
}

bool vws_form_set_handler(VwsForm *f, const char *id, const char *fn) {
    VwsControl *c = vws_form_find(f, id);
    if (!c) return false;
    snprintf(c->handler, sizeof c->handler, "%s", fn ? fn : "");
    return true;
}

bool vws_form_remove(VwsForm *f, const char *id) {
    if (!f || !id) return false;
    VwsControl **pp = &f->controls;
    while (*pp) {
        if (strcmp((*pp)->id, id) == 0) {
            VwsControl *dead = *pp;
            *pp = dead->next;
            free(dead);
            f->count--;
            return true;
        }
        pp = &(*pp)->next;
    }
    return false;
}

/* Reorder: move a control to the top of the z-order (the end of the list). */
void vws_form_bring_front(VwsForm *f, const char *id) {
    if (!f || !id) return;
    VwsControl **pp = &f->controls;
    while (*pp && strcmp((*pp)->id, id) != 0) pp = &(*pp)->next;
    if (!*pp || !(*pp)->next) return;         /* not found, or already last */
    VwsControl *c = *pp;
    *pp = c->next;                            /* unlink */
    VwsControl *p = f->controls;
    while (p->next) p = p->next;
    p->next = c;
    c->next = NULL;
}

/* --------------------------------------------------------------- palette */

const VwsPaletteItem *vws_palette(int *count) {
    static const VwsPaletteItem pal[] = {
        { "label",    "Label",    80, 20 },
        { "button",   "Button",   96, 28 },
        { "textbox",  "TextBox", 120, 24 },
        { "checkbox", "CheckBox",120, 24 },
        { "listbox",  "ListBox", 140, 96 },
        { "progress", "Progress",140, 20 },
        { "slider",   "Slider",  140, 24 },
        { "groupbox", "GroupBox",160, 96 },
        { "image",    "Image",    96, 96 },
        { "separator","Separator",120,  8 },
    };
    if (count) *count = (int)(sizeof pal / sizeof pal[0]);
    return pal;
}

/* ------------------------------------------------------------ error sink */

static char g_err[256];

const char *vws_form_error(void) { return g_err[0] ? g_err : NULL; }

void vws_form_set_error(const char *msg) {
    if (!msg) { g_err[0] = '\0'; return; }
    snprintf(g_err, sizeof g_err, "%s", msg);
}

/* ------------------------------------------------------- handler scanning */
/*
 * Find every top-level `function NAME(` in the source and capture its body up
 * to the matching `end`.  This is deliberately not a Lua parser: it counts
 * block openers (`function`, `if`, `for`, `while`, `do`) against `end`, which
 * is enough to find the extent of a handler without misreading strings.
 *
 * Returns the number captured, or -1 on an unterminated block.
 */

static bool matches_word(const char *s, long len, long i, const char *word) {
    long n = (long)strlen(word);
    if (i + n > len) return false;
    if (strncmp(s + i, word, (size_t)n) != 0) return false;
    /* must not be a longer identifier */
    if (i > 0) {
        char p = s[i - 1];
        if ((p >= 'a' && p <= 'z') || (p >= 'A' && p <= 'Z') ||
            (p >= '0' && p <= '9') || p == '_') return false;
    }
    if (i + n < len) {
        char q = s[i + n];
        if ((q >= 'a' && q <= 'z') || (q >= 'A' && q <= 'Z') ||
            (q >= '0' && q <= '9') || q == '_') return false;
    }
    return true;
}

/* Skip a string literal starting at s[i] (which must be a quote). */
static long skip_string(const char *s, long len, long i) {
    char q = s[i];
    i++;
    while (i < len) {
        if (s[i] == '\\') { i += 2; continue; }
        if (s[i] == q) return i + 1;
        i++;
    }
    return len;
}

int vws_scan_handlers(VwsSource *src, VwsForm *f) {
    if (!src || !src->src || !f) return -1;
    const char *s = src->src;
    long len = src->len;

    for (long i = 0; i < len; i++) {
        if (s[i] == '"' || s[i] == '\'') { i = skip_string(s, len, i) - 1; continue; }
        if (s[i] == '-' && i + 1 < len && s[i+1] == '-') {
            while (i < len && s[i] != '\n') i++;
            continue;
        }
        if (!matches_word(s, len, i, "function")) continue;

        long j = i + 8;
        while (j < len && (s[j] == ' ' || s[j] == '\t')) j++;
        long ns = j;
        while (j < len && ((s[j] >= 'a' && s[j] <= 'z') ||
                           (s[j] >= 'A' && s[j] <= 'Z') ||
                           (s[j] >= '0' && s[j] <= '9') || s[j] == '_')) j++;
        if (j == ns) { i = j; continue; }
        char name[128];
        long nlen = j - ns;
        if (nlen >= (long)sizeof name) nlen = sizeof name - 1;
        memcpy(name, s + ns, (size_t)nlen);
        name[nlen] = '\0';

        /* Find the extent of the body. */
        long depth = 0;
        long k = j;
        bool terminated = false;
        for (; k < len; k++) {
            if (s[k] == '"' || s[k] == '\'') { k = skip_string(s, len, k) - 1; continue; }
            if (s[k] == '-' && k + 1 < len && s[k+1] == '-') {
                while (k < len && s[k] != '\n') k++;
                continue;
            }
            if (matches_word(s, len, k, "function") ||
                matches_word(s, len, k, "if")      ||
                matches_word(s, len, k, "for")     ||
                matches_word(s, len, k, "while")) {
                depth++;
                k += 2;
                continue;
            }
            /* `do` only opens a block after while/for/repeat, but a bare `do`
             * block also opens one -- counting every `do` is safe here because
             * the matching `end` exists either way. */
            if (matches_word(s, len, k, "do")) { depth++; k += 1; continue; }
            if (matches_word(s, len, k, "end")) {
                depth--;
                if (depth <= 0) { terminated = true; k += 3; break; }
                k += 2;
                continue;
            }
        }
        if (!terminated) return -1;

        /* Store the body (without the `function NAME(` line). */
        long body_start = j;
        /* Skip to the end of the parameter list / line. */
        while (body_start < len && s[body_start] != '\n') body_start++;
        if (body_start < len) body_start++;

        char *body = (char *)malloc((size_t)(k - body_start) + 1);
        if (!body) return -1;
        long blen = k - body_start;
        if (blen > 0) memcpy(body, s + body_start, (size_t)blen);
        body[blen] = '\0';

        VwsHandler *grown = (VwsHandler *)realloc(f->handlers,
                              (size_t)(f->nhandlers + 1) * sizeof *grown);
        if (!grown) { free(body); return -1; }
        f->handlers = grown;
        snprintf(f->handlers[f->nhandlers].name, 128, "%s", name);
        f->handlers[f->nhandlers].body = body;
        f->nhandlers++;
        i = k;
    }
    return f->nhandlers;
}

void vws_source_free(VwsSource *src) {
    if (!src) return;
    free(src->src);
    src->src = NULL;
    src->len = 0;
}
