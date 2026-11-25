# VWS -- Visual WuBu System
#
# WuBu's Visual Basic: a form designer and code-behind editor over the .WET
# applet framework.  The language is Lua; VWS adds the LOOP.
#
#   make test      build and run the suite
#
# A .vws file IS a .wet Lua script, so there is no separate design format to
# keep in sync.

CC      ?= cc
CFLAGS  ?= -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra
LDLIBS  ?= -lm

# Which WET?  Inside WuBuOS, ../wet is the toolkit the rest of the OS links, so
# prefer it -- compiling the vendored copy instead is how this repo silently drifted one
# commit behind (see docs: vendor drift). Cloned on its own, there is no ../wet and the
# vendored copy is used, which keeps the repo buildable standalone.
# Override either way:  make WET=/path/to/wet test
WET ?= ../wet
WUA ?= $(WET)/externals/wua/src

# One copy, or none: this repo used to vendor its own wet+wua, which drifted a commit
# behind the toolkit WuBuOS links. If the toolkit is missing, say so by name rather than
# silently compiling an older copy.
ifeq ($(wildcard $(WET)/wet_widget.c),)
$(error no .WET toolkit at WET=$(WET); clone waefrebeorn/wet beside vws or pass WET=/path/to/wet)
endif
WUA_SRCS := $(WUA)/wb_lv_arena.c $(WUA)/wb_lv_coro.c $(WUA)/wb_lv_eval.c \
            $(WUA)/wb_lv_host.c $(WUA)/wb_lv_lex.c $(WUA)/wb_lv_parse.c \
            $(WUA)/wb_lv_value.c $(WUA)/wb_lv_stdlib.c $(WUA)/wb_lv_string.c

WET_SRCS := $(WET)/wet_widget.c $(WET)/wet_layout.c $(WET)/wet_render.c \
            $(WET)/wet_backend_vbe.c $(WET)/wet_controls.c $(WET)/wet_app.c \
            $(WET)/wet_input.c $(WET)/wet_lua.c

VWS_SRCS := vws_form.c vws_emit.c vws_load.c vws_templates.c

# The template library is PURE C11 over stdlib: the CLI links it alone, with no
# Lua and no .WET, so the IDE can do the same and get identical output.
VWS_TEMPLATE_SRCS := vws_templates.c

VWS_LUA_SRCS := vws_form.c vws_emit.c vws_load.c vws_templates.c vws_lua.c $(WET)/wet_lua.c

.PHONY: all test clean
all: build/test_vws build/test_vws_designer build/test_vws_templates build/vws

build:
	mkdir -p build

build/test_vws: vws_selftest.c $(VWS_SRCS) $(WET_SRCS) $(WUA_SRCS) \
                $(WET)/hosted/vbe_shim.c | build
	$(CC) $(CFLAGS) -I. -I$(WET) -I$(WUA) $^ $(LDLIBS) -o $@

# The designer applet: .WET + VWS in one interpreter.
build/test_vws_designer: vws_designer_test.c $(VWS_LUA_SRCS) $(WET_SRCS) \
                         $(WUA_SRCS) $(WET)/hosted/vbe_shim.c | build
	$(CC) $(CFLAGS) -I. -I$(WET) -I$(WUA) $^ $(LDLIBS) -o $@

# The template library's gate: every template emits, loads back through the
# real designer loader, and matches its own data.
build/test_vws_templates: vws_templates_test.c $(VWS_SRCS) $(WET_SRCS) \
                         $(WUA_SRCS) $(WET)/hosted/vbe_shim.c | build
	$(CC) $(CFLAGS) -I. -I$(WET) -I$(WUA) $^ $(LDLIBS) -o $@

# The CLI.  Links the pure template library only -- no Lua, no .WET -- so the
# IDE can shell out to it (or link the same file) with nothing else to drag in.
build/vws: vws_cli.c $(VWS_TEMPLATE_SRCS) | build
	$(CC) $(CFLAGS) -I. $^ $(LDLIBS) -o $@

test: build/test_vws build/test_vws_designer build/test_vws_templates build/vws
	./build/test_vws
	./build/test_vws_designer
	./build/test_vws_templates
	./build/vws --list >/dev/null

clean:
	rm -rf build
