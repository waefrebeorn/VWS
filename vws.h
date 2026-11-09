/*
 * vws.h -- Visual WuBu System.
 *
 * WuBu's Visual Basic: a form designer and code-behind editor over the .WET
 * applet framework.  The language is Lua (already the .WET surface); what VWS
 * adds is the LOOP -- drag a control onto a form, pick an event, write the
 * handler, run it.
 *
 * The file model follows VB's .frm: ONE text file, runnable and editable.
 * A .vws file is a plain .wet Lua script.  The designer loads it by running it
 * through .WET and walking the widget tree; it saves by emitting Lua.  There is
 * no second representation to drift.
 *
 * The designer's in-memory document is a VwsForm: an ordered list of controls
 * with a position, a size, a caption and an optional handler name.  Position is
 * absolute, which is exactly VB's model -- a .vws form emits a wet.canvas with
 * left/top on each child, so it reflows as a group but not per-control.
 */
#ifndef VWS_H
#define VWS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VwsControl VwsControl;
typedef struct VwsForm   VwsForm;

/* A user handler body recovered from the source.  The .frm guarantee: it is
 * re-emitted verbatim, so a re-save never clobbers user code. */
typedef struct VwsHandler {
    char name[128];
    char *body;
} VwsHandler;

/* --------------------------------------------------------------- controls */

struct VwsControl {
    VwsControl *next;

    char  id[64];        /* "Button1" -- what the script calls it  */
    char  cls[32];       /* "button", "label", ... -- a .WET element */
    int   left, top;     /* absolute, in form coordinates      */
    int   w, h;          /* size, in form coordinates          */
    char  text[256];     /* caption / initial text             */
    char  handler[128];  /* Lua function name for onclick, or "" */
    bool  selected;      /* designer selection state           */
};

/* ------------------------------------------------------------------ form */

struct VwsForm {
    char        title[128];
    int         width, height;
    VwsControl *controls;      /* in z-order, bottom first */
    int         count;

    /* Top-level `function NAME(...)` bodies found in the source, kept so a
     * re-emit never clobbers user code.  This is the VB .frm guarantee. */
    VwsHandler *handlers;
    int         nhandlers;
};

/* -------------------------------------------------------------- lifecycle */

VwsForm *vws_form_new(const char *title, int width, int height);
void     vws_form_free(VwsForm *f);

/* --------------------------------------------------------- designer edits */

VwsControl *vws_form_add(VwsForm *f, const char *id, const char *cls,
                         int left, int top, int w, int h);
VwsControl *vws_form_find(VwsForm *f, const char *id);
VwsControl *vws_form_at(VwsForm *f, int x, int y);   /* topmost control */
bool        vws_form_move(VwsForm *f, const char *id, int left, int top);
bool        vws_form_resize(VwsForm *f, const char *id, int w, int h);
bool        vws_form_set_text(VwsForm *f, const char *id, const char *text);
bool        vws_form_set_handler(VwsForm *f, const char *id, const char *fn);
bool        vws_form_remove(VwsForm *f, const char *id);
void        vws_form_bring_front(VwsForm *f, const char *id);

/* ---------------------------------------------------------- file handling */

/* Emit the form as a runnable .wet Lua script.  malloc'd; caller frees. */
char *vws_form_emit(const VwsForm *f);

/* Load a .vws file: run it through .WET, walk the tree back into a form, and
 * collect any user function bodies so a re-emit preserves them. */
VwsForm *vws_form_load(const char *path, char *err, size_t err_cap);

/* The last load/emit error, or NULL. */
const char *vws_form_error(void);

/* -------------------------------------------------------------- metadata */

/* The palette: the control classes a designer offers, in display order. */
typedef struct { const char *cls; const char *label; int def_w, def_h; } VwsPaletteItem;
const VwsPaletteItem *vws_palette(int *count);

#ifdef __cplusplus
}
#endif

#endif /* VWS_H */
