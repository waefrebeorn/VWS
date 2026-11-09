/*
 * vws_lua.h -- the Lua face of the VWS document model.
 *
 * The designer is written in Lua over .WET, and it drives the SAME C document
 * the loader and emitter use.  One model, three surfaces: the designer, the
 * file loader, and the file emitter.  A control added in Lua is a control the
 * emitter will write and the loader will read back.
 *
 * This is what makes the stack circular rather than merely layered: the tool
 * that builds the GUI is built from the GUI framework, in the language the
 * framework exposes.
 */
#ifndef VWS_LUA_H
#define VWS_LUA_H

#include "vws.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VwsLua VwsLua;

/* Create the state, register `vws.*` in it, and create an empty document.
 * `path` may be NULL, which starts a new form. */
/* Create a state, register vws.* in it, and create an empty document.
 * `path` may be NULL, which starts a new form. */
VwsLua *vws_lua_new(const char *title, int width, int height,
                    char *err, size_t err_cap);
void    vws_lua_free(VwsLua *vl);

/* Wrap an EXISTING interpreter state -- for a host (a designer applet) that
 * already has a Lua state running .WET and wants the VWS document in it too.
 * The state is not owned: it is not freed by vws_lua_free. */
VwsLua *vws_lua_open(void *L, const char *title, int width, int height,
                     char *err, size_t err_cap);

/* The document, so a C host can reach it too. */
VwsForm *vws_lua_form(VwsLua *vl);

/* Load a .vws file into the Lua document.  Returns NULL on success or a static
 * error string. */
const char *vws_lua_load(VwsLua *vl, const char *path);

/* Emit the document to a .wet Lua script.  malloc'd; caller frees. */
char *vws_lua_emit(VwsLua *vl);

/* The raw interpreter state, for a designer that wants to run a script. */
struct wb_lv *vws_lua_state(VwsLua *vl);

const char *vws_lua_error(VwsLua *vl);

#ifdef __cplusplus
}
#endif

#endif /* VWS_LUA_H */
