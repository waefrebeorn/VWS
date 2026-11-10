/*
 * vws_templates.h -- the VWS template library.
 *
 * Every program in WuBuOS is created from a template.  A template is a named,
 * complete, runnable .wet Lua script -- not a skeleton the user has to finish.
 * Loading one into the designer yields a real form with real controls and
 * handler stubs that already do something visible, so "New from template" is
 * also "Run" the moment it is created.
 *
 * This file is deliberately PURE C11 over stdlib only: it knows nothing about
 * Lua, .WET or the VWS document model.  That is what lets the IDE link it
 * directly and get byte-identical output to the designer -- one owner for the
 * template text, never a second copy in the editor.
 *
 * A .vws file IS a .wet Lua script, so a template's output loads back through
 * vws_form_load unchanged.  The emit shape here mirrors vws_emit.c exactly.
 */
#ifndef VWS_TEMPLATES_H
#define VWS_TEMPLATES_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One named template, as a picker shows it. */
typedef struct {
    const char *name;        /* "blank" -- what the CLI and the IDE pass */
    const char *label;       /* "Blank Applet" -- what the user reads     */
    const char *description; /* one line, shown under the label           */
    int         def_w, def_h;
} VwsTemplate;

/* The library, in picker order. */
int                vws_template_count(void);
const VwsTemplate *vws_template_at(int i);
const VwsTemplate *vws_template_find(const char *name);

/* Emit a template as a complete runnable .wet Lua script.
 * Returns a malloc'd NUL-terminated string (caller frees), or NULL when the
 * name is unknown or the allocation fails. */
char *vws_template_emit(const char *name);

/* Convenience for callers that build a picker: the label/description pairs. */
const char *vws_template_label(const char *name);
const char *vws_template_description(const char *name);
const char *vws_template_title(const char *name);

/* Walk a template's controls in order.  The control table stays private to
 * vws_templates.c; this is how the designer builds a document from a template
 * without parsing emitted text and without a second copy of the data. */
typedef void (*VwsTemplateCtrlFn)(void *ud, const char *id, const char *cls,
                                  const char *text, const char *handler,
                                  int left, int top, int w, int h);
void vws_template_foreach_ctrl(const char *name, VwsTemplateCtrlFn fn, void *ud);

#ifdef __cplusplus
}
#endif

#endif /* VWS_TEMPLATES_H */
