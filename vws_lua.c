/*
 * vws_lua.c -- the Lua face of the VWS document model.
 *
 * The designer is written in Lua over .WET and drives the SAME C document the
 * loader and emitter use, so there is exactly one model and no second
 * representation to drift.  Keys are interned because the interpreter hashes
 * table keys by string identity -- a plain wb_lv_str("left") would never match
 * a script literal.
 */
#include "vws_internal.h"
#include "vws_lua.h"

#include "wb_lv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VwsLuaCtx VwsLuaCtx;

struct VwsLua {
    VwsLuaCtx *ctx;
};

struct VwsLuaCtx {
    wb_lv   *L;
    VwsForm *form;
    bool     owns_state;   /* true for vws_lua_new, false for vws_lua_open */
};

static VwsLuaCtx *g_ctx;

/* An interned key, so lookups match script literals. */
static wb_lv_value k(wb_lv *L, const char *s) {
    return wb_lv_str(wb_lv_intern(L, s, strlen(s)));
}

static VwsControl *find(VwsLuaCtx *c, const char *id) {
    return c ? vws_form_find(c->form, id) : NULL;
}

static int num(wb_lv_value v, int *ok) { return (int)wb_lv_tointeger(v, ok); }

/* ------------------------------------------------------------- document */

static wb_lv_value fn_add(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    if (n < 7) { wb_lv_set_global(L, "__vws_err", wb_lv_str("add needs id, cls, left, top, w, h")); return wb_lv_bool(0); }
    const char *id  = wb_lv_tostring(a[0]);
    const char *cls = wb_lv_tostring(a[1]);
    int ok1 = 0, ok2 = 0, ok3 = 0, ok4 = 0;
    int left = num(a[2], &ok1), top = num(a[3], &ok2);
    int w = num(a[4], &ok3), h = num(a[5], &ok4);
    if (!id || !cls || !ok1 || !ok2 || !ok3 || !ok4) {
        wb_lv_set_global(L, "__vws_err", wb_lv_str("add: bad arguments"));
        return wb_lv_bool(0);
    }
    VwsControl *ctl = vws_form_add(c->form, id, cls, left, top, w, h);
    if (!ctl) {
        /* A duplicate id is the common failure; say so. */
        wb_lv_set_global(L, "__vws_err",
                         wb_lv_str(find(c, id) ? "add: id already exists" : "add: out of memory"));
        return wb_lv_bool(0);
    }
    wb_lv_value out; out.t = WB_LV_STR;
    out.as.s = ctl->id; out.slen = strlen(ctl->id);
    return out;
}

static wb_lv_value fn_count(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)a; (void)n;
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    return wb_lv_integer(c ? c->form->count : 0);
}

static wb_lv_value fn_remove(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L;
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    if (n < 1) return wb_lv_bool(0);
    return wb_lv_bool(vws_form_remove(c->form, wb_lv_tostring(a[0])));
}

static wb_lv_value fn_move(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L;
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    if (n < 3) return wb_lv_bool(0);
    int ok1 = 0, ok2 = 0;
    return wb_lv_bool(vws_form_move(c->form, wb_lv_tostring(a[0]),
                                    num(a[1], &ok1), num(a[2], &ok2)));
}

static wb_lv_value fn_resize(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L;
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    if (n < 3) return wb_lv_bool(0);
    int ok1 = 0, ok2 = 0;
    return wb_lv_bool(vws_form_resize(c->form, wb_lv_tostring(a[0]),
                                      num(a[1], &ok1), num(a[2], &ok2)));
}

static wb_lv_value fn_settext(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L;
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    if (n < 2) return wb_lv_bool(0);
    return wb_lv_bool(vws_form_set_text(c->form, wb_lv_tostring(a[0]),
                                        wb_lv_tostring(a[1])));
}

static wb_lv_value fn_sethandler(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L;
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    if (n < 2) return wb_lv_bool(0);
    return wb_lv_bool(vws_form_set_handler(c->form, wb_lv_tostring(a[0]),
                                           wb_lv_tostring(a[1])));
}

static wb_lv_value fn_bringfront(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L;
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    if (n < 1) return wb_lv_nil();
    vws_form_bring_front(c->form, wb_lv_tostring(a[0]));
    return wb_lv_nil();
}

static wb_lv_value fn_at(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    if (n < 2) return wb_lv_nil();
    int ok1 = 0, ok2 = 0;
    VwsControl *ctl = vws_form_at(c->form, num(a[0], &ok1), num(a[1], &ok2));
    if (!ctl) return wb_lv_nil();
    wb_lv_value out; out.t = WB_LV_STR;
    out.as.s = ctl->id; out.slen = strlen(ctl->id);
    return out;
}

static wb_lv_value fn_clear(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)L; (void)a; (void)n;
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    while (c->form->controls) vws_form_remove(c->form, c->form->controls->id);
    return wb_lv_nil();
}

/* ------------------------------------------------------------- property */

static wb_lv_value fn_get(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    if (n < 2) return wb_lv_nil();
    VwsControl *ctl = find(c, wb_lv_tostring(a[0]));
    if (!ctl) return wb_lv_nil();
    const char *field = wb_lv_tostring(a[1]);
    if (!field) return wb_lv_nil();

    if (strcmp(field, "id") == 0)   { wb_lv_value v; v.t = WB_LV_STR; v.as.s = ctl->id;   v.slen = strlen(ctl->id);   return v; }
    if (strcmp(field, "class") == 0){ wb_lv_value v; v.t = WB_LV_STR; v.as.s = ctl->cls;  v.slen = strlen(ctl->cls);  return v; }
    if (strcmp(field, "text") == 0) { wb_lv_value v; v.t = WB_LV_STR; v.as.s = ctl->text; v.slen = strlen(ctl->text); return v; }
    if (strcmp(field, "handler") == 0) { wb_lv_value v; v.t = WB_LV_STR; v.as.s = ctl->handler; v.slen = strlen(ctl->handler); return v; }
    if (strcmp(field, "left") == 0)  return wb_lv_integer(ctl->left);
    if (strcmp(field, "top") == 0)   return wb_lv_integer(ctl->top);
    if (strcmp(field, "w") == 0)     return wb_lv_integer(ctl->w);
    if (strcmp(field, "h") == 0)     return wb_lv_integer(ctl->h);
    return wb_lv_nil();
}

static wb_lv_value fn_set(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    if (n < 3) return wb_lv_bool(0);
    VwsControl *ctl = find(c, wb_lv_tostring(a[0]));
    if (!ctl) return wb_lv_bool(0);
    const char *field = wb_lv_tostring(a[1]);
    if (!field) return wb_lv_bool(0);

    if (strcmp(field, "left") == 0)  { int ok=0; ctl->left = num(a[2], &ok); return wb_lv_bool(ok); }
    if (strcmp(field, "top") == 0)   { int ok=0; ctl->top  = num(a[2], &ok); return wb_lv_bool(ok); }
    if (strcmp(field, "w") == 0)     { int ok=0; ctl->w    = num(a[2], &ok); return wb_lv_bool(ok); }
    if (strcmp(field, "h") == 0)     { int ok=0; ctl->h    = num(a[2], &ok); return wb_lv_bool(ok); }
    if (strcmp(field, "text") == 0) { snprintf(ctl->text, sizeof ctl->text, "%s", wb_lv_tostring(a[2])); return wb_lv_bool(1); }
    if (strcmp(field, "handler") == 0) { snprintf(ctl->handler, sizeof ctl->handler, "%s", wb_lv_tostring(a[2])); return wb_lv_bool(1); }
    return wb_lv_bool(0);
}

/* The whole document as an array, so a designer can list it. */
static wb_lv_value fn_controls(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)a; (void)n;
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    wb_lv_table *arr = wb_lv_table_new(L, c->form->count, 0);
    int i = 0;
    for (VwsControl *ctl = c->form->controls; ctl; ctl = ctl->next) {
        wb_lv_table *t = wb_lv_table_new(L, 0, 8);
        #define S(f, v) wb_lv_table_set(L, t, k(L, f), (v))
        S("id", ((wb_lv_value){.t=WB_LV_STR, .as.s=ctl->id,   .slen=strlen(ctl->id)}));
        S("class", ((wb_lv_value){.t=WB_LV_STR, .as.s=ctl->cls,  .slen=strlen(ctl->cls)}));
        S("text", ((wb_lv_value){.t=WB_LV_STR, .as.s=ctl->text, .slen=strlen(ctl->text)}));
        S("handler", ((wb_lv_value){.t=WB_LV_STR, .as.s=ctl->handler, .slen=strlen(ctl->handler)}));
        S("left",    wb_lv_integer(ctl->left));
        S("top",     wb_lv_integer(ctl->top));
        S("w",       wb_lv_integer(ctl->w));
        S("h",       wb_lv_integer(ctl->h));
        #undef S
        wb_lv_table_set(L, arr, wb_lv_integer(i + 1),
                        (wb_lv_value){.t=WB_LV_TABLE, .as.tab=t});
        i++;
    }
    wb_lv_value out; out.t = WB_LV_TABLE; out.as.tab = arr;
    return out;
}

/* -------------------------------------------------------------- file I/O */

static wb_lv_value fn_emit(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)a; (void)n;
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    char *src = vws_form_emit(c->form);
    if (!src) return wb_lv_nil();
    wb_lv_value out; out.t = WB_LV_STR; out.as.s = src; out.slen = strlen(src);
    /* The value points at malloc'd memory the script now owns a copy of. */
    return out;
}

static wb_lv_value fn_err(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)a; (void)n;
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    wb_lv_value e = wb_lv_get_global(L, "__vws_err");
    if (e.t == WB_LV_STR) {
        wb_lv_set_global(L, "__vws_err", wb_lv_nil());
        return e;
    }
    return wb_lv_nil();
}

static wb_lv_value fn_save(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    if (n < 1) return wb_lv_bool(0);
    const char *path = wb_lv_tostring(a[0]);
    char *src = vws_form_emit(c->form);
    if (!src) { wb_lv_set_global(L, "__vws_err", wb_lv_str("save: emit failed")); return wb_lv_bool(0); }
    FILE *f = fopen(path, "wb");
    if (!f) { free(src); wb_lv_set_global(L, "__vws_err", wb_lv_str("save: cannot open")); return wb_lv_bool(0); }
    size_t n2 = fwrite(src, 1, strlen(src), f);
    fclose(f);
    free(src);
    if (n2 == 0) { wb_lv_set_global(L, "__vws_err", wb_lv_str("save: write failed")); return wb_lv_bool(0); }
    return wb_lv_bool(1);
}

static wb_lv_value fn_load(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    VwsLuaCtx *c = (VwsLuaCtx *)ud;
    if (n < 1) return wb_lv_bool(0);
    const char *path = wb_lv_tostring(a[0]);
    char err[256] = {0};
    VwsForm *f = vws_form_load(path, err, sizeof err);
    if (!f) {
        wb_lv_set_global(L, "__vws_err", wb_lv_str(err[0] ? err : "load failed"));
        return wb_lv_bool(0);
    }
    /* Swap the loaded document in. */
    vws_form_free(c->form);
    c->form = f;
    return wb_lv_bool(1);
}

static wb_lv_value fn_palette(wb_lv *L, wb_lv_value *a, int n, void *ud) {
    (void)a; (void)n; (void)ud;
    int count = 0;
    const VwsPaletteItem *pal = vws_palette(&count);
    wb_lv_table *arr = wb_lv_table_new(L, count, 0);
    for (int i = 0; i < count; i++) {
        wb_lv_table *t = wb_lv_table_new(L, 0, 4);
        wb_lv_table_set(L, t, k(L, "class"),
                        (wb_lv_value){.t=WB_LV_STR, .as.s=pal[i].cls, .slen=strlen(pal[i].cls)});
        wb_lv_table_set(L, t, k(L, "label"),
                        (wb_lv_value){.t=WB_LV_STR, .as.s=pal[i].label, .slen=strlen(pal[i].label)});
        wb_lv_table_set(L, t, k(L, "w"), wb_lv_integer(pal[i].def_w));
        wb_lv_table_set(L, t, k(L, "h"), wb_lv_integer(pal[i].def_h));
        wb_lv_table_set(L, arr, wb_lv_integer(i + 1),
                        (wb_lv_value){.t=WB_LV_TABLE, .as.tab=t});
    }
    wb_lv_value out; out.t = WB_LV_TABLE; out.as.tab = arr;
    return out;
}

/* ------------------------------------------------------------ lifecycle */

static void register_all(wb_lv *L, VwsLuaCtx *c) {
    static const struct { const char *name; wb_lv_cfn fn; } fns[] = {
        { "add",       fn_add },
        { "count",     fn_count },
        { "remove",    fn_remove },
        { "move",      fn_move },
        { "resize",    fn_resize },
        { "settext",   fn_settext },
        { "sethandler",fn_sethandler },
        { "bringfront",fn_bringfront },
        { "at",        fn_at },
        { "clear",     fn_clear },
        { "get",       fn_get },
        { "set",       fn_set },
        { "controls",  fn_controls },
        { "emit",      fn_emit },
        { "load",      fn_load },
        { "save",      fn_save },
        { "err",       fn_err },
        { "palette",   fn_palette },
        { NULL, NULL }
    };
    /* `vws` is a REAL table, so `vws.palette()` is a field access on it.
     * Registering a global literally named "vws.palette" would leave `vws`
     * nil and every call dead on line 1.  Keys are interned because the
     * interpreter hashes table keys by string identity. */
    wb_lv_table *vws = wb_lv_table_new(L, 0, 20);
    if (!vws) return;

#define VWS_KEY(name) wb_lv_str(wb_lv_intern(L, (name), strlen(name)))
#define VWS_FN(name, fn) \
    { wb_lv_value v; v.t = WB_LV_CFUNCTION; v.as.cf.f = (fn); v.as.cf.ud = c; \
      wb_lv_table_set(L, vws, VWS_KEY(name), v); }
    for (int i = 0; fns[i].name; i++) VWS_FN(fns[i].name, fns[i].fn);
#undef VWS_KEY
#undef VWS_FN

    wb_lv_value wv; wv.t = WB_LV_TABLE; wv.as.tab = vws;
    wb_lv_set_global(L, "vws", wv);

    /* Bare names too, for scripts that prefer them. */
    for (int i = 0; fns[i].name; i++)
        wb_lv_register_global(L, fns[i].name, fns[i].fn, c);
}

VwsLua *vws_lua_open(void *L, const char *title, int width, int height,
                     char *err, size_t err_cap) {
    if (!L) { if (err) snprintf(err, err_cap, "no lua state"); return NULL; }
    VwsLuaCtx *c = (VwsLuaCtx *)calloc(1, sizeof *c);
    if (!c) return NULL;
    c->L = (wb_lv *)L;
    c->owns_state = false;
    c->form = vws_form_new(title, width, height);
    if (!c->form) { wb_lv_free(c->L); free(c); return NULL; }

    VwsLua *vl = (VwsLua *)calloc(1, sizeof *vl);
    if (!vl) { vws_form_free(c->form); wb_lv_free(c->L); free(c); return NULL; }
    vl->ctx = c;
    g_ctx = c;
    register_all(c->L, c);
    return vl;
}

VwsLua *vws_lua_new(const char *title, int width, int height,
                    char *err, size_t err_cap) {
    wb_lv *L = wb_lv_new();
    if (!L) { if (err) snprintf(err, err_cap, "lua state failed"); return NULL; }
    VwsLua *vl = vws_lua_open(L, title, width, height, err, err_cap);
    if (!vl) { wb_lv_free(L); return NULL; }
    return vl;
}

void vws_lua_free(VwsLua *vl) {
    if (!vl) return;
    VwsLuaCtx *c = vl->ctx;
    if (!c) { free(vl); return; }
    vws_form_free(c->form);
    if (c->L && c->owns_state) wb_lv_free(c->L);
    if (g_ctx == c) g_ctx = NULL;
    free(c);
    free(vl);
}

VwsForm *vws_lua_form(VwsLua *vl) { return (vl && vl->ctx) ? vl->ctx->form : NULL; }

char *vws_lua_emit(VwsLua *vl) {
    return (vl && vl->ctx) ? vws_form_emit(vl->ctx->form) : NULL;
}

const char *vws_lua_load(VwsLua *vl, const char *path) {
    if (!vl || !vl->ctx || !path) return "no document or path";
    char err[256] = {0};
    VwsForm *f = vws_form_load(path, err, sizeof err);
    if (!f) return err[0] ? err : "load failed";
    vws_form_free(vl->ctx->form);
    vl->ctx->form = f;
    return NULL;
}

struct wb_lv *vws_lua_state(VwsLua *vl) {
    return (vl && vl->ctx) ? vl->ctx->L : NULL;
}

const char *vws_lua_error(VwsLua *vl) {
    if (!vl || !vl->ctx || !vl->ctx->L) return NULL;
    wb_lv_value e = wb_lv_get_global(vl->ctx->L, "__vws_err");
    return (e.t == WB_LV_STR) ? e.as.s : NULL;
}
