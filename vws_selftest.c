/*
 * vws_selftest.c -- behaviour gates for Visual WuBu System.
 *
 * The contract under test is the round trip: a designer document, emitted to
 * Lua, runs through .WET, and loads back into the same document.  If that
 * round trip holds, the designer's view and the running applet cannot drift.
 */
#include "vws_internal.h"

#include "wet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks, g_fails;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

static const char *TMP = "/tmp/vws_roundtrip.vws";

static void write_tmp(const char *src) {
    FILE *f = fopen(TMP, "wb");
    if (!f) return;
    fwrite(src, 1, strlen(src), f);
    fclose(f);
}

/* ------------------------------------------------------------- document */

static void test_document(void) {
    VwsForm *f = vws_form_new("Form1", 400, 300);
    CHECK(f != NULL, "form created");
    CHECK(strcmp(f->title, "Form1") == 0, "title stored");
    CHECK(f->width == 400 && f->height == 300, "size stored");

    VwsControl *b = vws_form_add(f, "Button1", "button", 24, 40, 100, 28);
    CHECK(b != NULL, "button added");
    CHECK(f->count == 1, "count is one");
    CHECK(vws_form_add(f, "Button1", "button", 0, 0, 10, 10) == NULL,
          "duplicate id rejected");

    vws_form_set_text(f, "Button1", "OK");
    CHECK(strcmp(b->text, "OK") == 0, "caption set");
    vws_form_set_handler(f, "Button1", "Button1_Click");
    CHECK(strcmp(b->handler, "Button1_Click") == 0, "handler set");

    vws_form_add(f, "Label1", "label", 24, 80, 120, 20);
    CHECK(f->count == 2, "second control added");

    /* hit-testing is topmost-wins */
    CHECK(vws_form_at(f, 30, 50) == b, "hit finds the button");
    CHECK(vws_form_at(f, 5, 5) == NULL, "miss outside every control");

    /* z-order: the last-added control paints on top, and a hit on the overlap
     * resolves to it.  Label1 was added after Button1, so it wins. */
    vws_form_move(f, "Label1", 24, 40);
    CHECK(vws_form_at(f, 30, 50) == vws_form_find(f, "Label1"),
          "overlap resolves to the topmost (label)");
    vws_form_bring_front(f, "Button1");
    CHECK(vws_form_at(f, 30, 50) == b, "bring_front makes the button topmost");

    /* move / resize / remove */
    CHECK(vws_form_move(f, "Button1", 10, 10), "move accepted");
    CHECK(b->left == 10 && b->top == 10, "move applied");
    CHECK(vws_form_resize(f, "Button1", 200, 50), "resize accepted");
    CHECK(b->w == 200 && b->h == 50, "resize applied");
    CHECK(vws_form_remove(f, "Button1"), "remove accepted");
    CHECK(f->count == 1, "count after remove");
    CHECK(vws_form_find(f, "Button1") == NULL, "removed id no longer found");

    vws_form_free(f);
}

static void test_palette(void) {
    int n = 0;
    const VwsPaletteItem *pal = vws_palette(&n);
    CHECK(n >= 8, "palette has the standard controls");
    CHECK(pal != NULL, "palette returned");
    bool has_button = false, has_textbox = false;
    for (int i = 0; i < n; i++) {
        if (strcmp(pal[i].cls, "button") == 0) has_button = true;
        if (strcmp(pal[i].cls, "textbox") == 0) has_textbox = true;
    }
    CHECK(has_button && has_textbox, "palette carries button and textbox");
}

/* --------------------------------------------------------------- emitter */

static void test_emit(void) {
    VwsForm *f = vws_form_new("Form1", 400, 300);
    vws_form_add(f, "Button1", "button", 24, 40, 100, 28);
    vws_form_set_text(f, "Button1", "OK");

    char *src = vws_form_emit(f);
    CHECK(src != NULL, "emit produced source");
    if (!src) { vws_form_free(f); return; }

    CHECK(strstr(src, "wet.applet") != NULL, "emitted an applet");
    CHECK(strstr(src, "wet.canvas") != NULL, "emitted a canvas (absolute layout)");
    CHECK(strstr(src, "wet.button { id=") != NULL, "emitted the button element");
    CHECK(strstr(src, "left=24, top=40") != NULL, "emitted absolute position");
    CHECK(strstr(src, "w=100, h=28") != NULL, "emitted the size");
    CHECK(strstr(src, "title = ") != NULL, "emitted the title");

    /* Escaping: a caption with a quote must not break the Lua string. */
    vws_form_set_text(f, "Button1", "It's \"OK\"");
    char *src2 = vws_form_emit(f);
    /* the emitter uses double-quoted Lua strings, so " must be escaped */
    CHECK(src2 && strstr(src2, "\\\"OK\\\"") != NULL,
          "embedded quote escaped in the caption");

    free(src);
    free(src2);
    vws_form_free(f);
}

/* ------------------------------------------------------------ round trip */

static void test_round_trip(void) {
    VwsForm *f = vws_form_new("Form1", 400, 300);
    vws_form_add(f, "Button1", "button", 24, 40, 100, 28);
    vws_form_set_text(f, "Button1", "OK");
    vws_form_add(f, "TextBox1", "textbox", 24, 80, 140, 24);
    vws_form_set_text(f, "TextBox1", "hello");
    vws_form_add(f, "Label1", "label", 24, 120, 120, 20);

    char *src = vws_form_emit(f);
    CHECK(src != NULL, "emitted");
    if (!src) { vws_form_free(f); return; }

    /* A handler body must survive the round trip verbatim. */
    const char *with_handler =
        "-- Form1.vws -- Visual WuBu System\n"
        "function Button1_Click(self)\n"
        "  wet.messagebox('hi', 'Form1')\n"
        "end\n\n";
    size_t n = strlen(with_handler) + strlen(src) + 1;
    char *joined = (char *)malloc(n);
    if (joined) {
        snprintf(joined, n, "%s%s", with_handler, src);
        write_tmp(joined);
        free(joined);
    } else {
        write_tmp(src);
    }

    char err[256] = {0};
    VwsForm *back = vws_form_load(TMP, err, sizeof err);
    CHECK(back != NULL, err[0] ? err : "loaded back");
    if (!back) { free(src); vws_form_free(f); return; }

    CHECK(back->width == 400 && back->height == 300, "size survived");
    CHECK(back->count == 3, "all three controls survived");

    VwsControl *b = vws_form_find(back, "Button1");
    CHECK(b != NULL, "Button1 found after load");
    if (b) {
        CHECK(b->left == 24 && b->top == 40, "position survived");
        CHECK(b->w == 100 && b->h == 28, "size survived");
        CHECK(strcmp(b->text, "OK") == 0, "caption survived");
        CHECK(strcmp(b->cls, "button") == 0, "class survived");
    }
    VwsControl *t = vws_form_find(back, "TextBox1");
    CHECK(t && strcmp(t->text, "hello") == 0, "textbox caption survived");

    /* The handler body is the .frm guarantee: re-emitting must keep it. */
    CHECK(back->nhandlers == 1, "one handler captured");
    if (back->nhandlers == 1) {
        CHECK(strcmp(back->handlers[0].name, "Button1_Click") == 0, "handler name");
        CHECK(back->handlers[0].body && strstr(back->handlers[0].body, "messagebox") != NULL,
              "handler body preserved verbatim");
    }

    /* Re-emit and confirm the handler is still there. */
    char *again = vws_form_emit(back);
    CHECK(again != NULL, "re-emit produced source");
    if (again) {
        CHECK(strstr(again, "Button1_Click") != NULL, "re-emit kept the handler name");
        CHECK(strstr(again, "messagebox") != NULL, "re-emit kept the handler body");
        free(again);
    }

    vws_form_free(back);
    free(src);
    vws_form_free(f);
}

static void test_load_bad_path(void) {
    char err[256] = {0};
    VwsForm *f = vws_form_load("/nonexistent/nope.vws", err, sizeof err);
    CHECK(f == NULL, "bad path returns NULL");
    CHECK(err[0] != '\0', "bad path reports an error");
    CHECK(vws_form_error() != NULL, "error is readable");
}

static void test_load_bad_lua(void) {
    write_tmp("this is not lua at all ((( \n");
    char err[256] = {0};
    VwsForm *f = vws_form_load(TMP, err, sizeof err);
    CHECK(f == NULL, "bad lua returns NULL");
    CHECK(err[0] != '\0', "bad lua reports an error");
}

static void test_load_no_applet(void) {
    write_tmp("-- just a comment, no applet\n");
    char err[256] = {0};
    VwsForm *f = vws_form_load(TMP, err, sizeof err);
    CHECK(f != NULL, "a file with no applet still loads");
    if (f) {
        CHECK(f->count == 0, "and yields an empty form");
        vws_form_free(f);
    }
}

int main(void) {
    printf("=== vws: visual wubu system ===\n");
    test_document();
    test_palette();
    test_emit();
    test_round_trip();
    test_load_bad_path();
    test_load_bad_lua();
    test_load_no_applet();
    printf("=== vws: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
