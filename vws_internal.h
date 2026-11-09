/*
 * vws_internal.h -- VWS internals.  Not for inclusion outside the repo.
 *
 * Split by concern: vws_form.c owns the document and the designer edits,
 * vws_emit.c owns turning a document into Lua, vws_load.c owns turning a built
 * .WET applet back into a document.
 */
#ifndef VWS_INTERNAL_H
#define VWS_INTERNAL_H

#include "vws.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Set the form error sink (vws_form.c owns the buffer). */
void vws_form_set_error(const char *msg);

/* ---------------------------------------------------------- source scanning */
/*
 * The .frm guarantee: user code survives a re-emit.  A .vws file is Lua, so a
 * top-level `function NAME(...)` is a handler.  The scanner captures each body
 * verbatim -- it does not parse Lua, it tracks block nesting well enough to
 * find the matching `end`.
 */

typedef struct {
    char *src;         /* the whole file, owned          */
    long  len;
} VwsSource;

int  vws_scan_handlers(VwsSource *src, VwsForm *f);
void vws_source_free(VwsSource *src);

/* --------------------------------------------------------------- emitter */

void vws_emit_applet(const VwsForm *f, VwsSource *handlers, char **out);

/* ---------------------------------------------------------------- loader */

VwsForm *vws_load_from_app(void *wet_app, VwsSource *src);

#endif /* VWS_INTERNAL_H */
