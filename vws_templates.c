/*
 * vws_templates.c -- the template library's data and its emitter.
 *
 * Templates are DATA, not eight copy-pasted scripts: one table of controls,
 * one table of handler stubs, and one generic emitter.  Adding a template is
 * adding rows.  The emitter's output shape mirrors vws_emit.c exactly, so a
 * template loads back through vws_form_load and re-saves unchanged.
 *
 * PURE C11 over stdlib only -- no Lua, no .WET, no VWS document model.  The IDE
 * links this file directly and emits byte-identical template text to the
 * designer; there is no second copy of a template anywhere.
 */
#include "vws_templates.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------- template data */

typedef struct {
    const char *id;
    const char *cls;
    const char *text;
    const char *handler;     /* Lua function name, or NULL */
    int         left, top, w, h;
} TCtrl;

typedef struct {
    const char *name;
    const char *body;        /* emitted verbatim between function/end */
} THandler;

typedef struct {
    /* The leading fields are byte-for-byte VwsTemplate: the accessors hand
     * a TDef* out as a VwsTemplate*, so the public struct MUST lead. */
    const char *name;
    const char *label;
    const char *description;
    int         def_w, def_h;
    const char *title;       /* applet title                       */
    const TCtrl     *ctrls;
    int              nctrls;
    const THandler  *handlers;
    int              nhandlers;
} TDef;

/* --- Blank ------------------------------------------------------------- */
static const TDef T_BLANK = {
    "blank", "Blank Applet", "An empty applet: a title, a canvas, nothing else.",
    320, 200, "Untitled", NULL, 0, NULL, 0,
};

/* --- Dialog ------------------------------------------------------------ */
static const TCtrl DIALOG_CTRLS[] = {
    { "Prompt",  "label",    "Your text here", NULL,         32,  40, 200, 20 },
    { "Answer",  "textbox",  "",               NULL,         32,  68, 296, 24 },
    { "OK",      "button",   "OK",             "OK_Click",   200, 116,  64, 28 },
    { "Cancel",  "button",   "Cancel",         "Cancel_Click",272,116,  64, 28 },
};
static const THandler DIALOG_HANDLERS[] = {
    { "OK_Click",     "  wet.messagebox(\"OK\", wet.find(\"Answer\").text or \"\")\n" },
    { "Cancel_Click", "  wet.messagebox(\"Cancelled\", \"Dialog\")\n" },
};
static const TDef T_DIALOG = {
    "dialog", "Dialog", "A prompt, a text box, and OK / Cancel.",
    360, 160, "Dialog",
    DIALOG_CTRLS, (int)(sizeof DIALOG_CTRLS / sizeof DIALOG_CTRLS[0]),
    DIALOG_HANDLERS, (int)(sizeof DIALOG_HANDLERS / sizeof DIALOG_HANDLERS[0]),
};

/* --- List and detail --------------------------------------------------- */
static const TCtrl LIST_CTRLS[] = {
    { "Items",   "listbox",  "",          "Items_Select", 16,  16, 200, 200 },
    { "Detail",  "groupbox", "Detail",    NULL,           232, 16, 232, 120 },
    { "DetailText", "label", "",          NULL,           248, 48, 200, 60 },
    { "Add",     "button",   "Add",       "Add_Click",    232, 152, 110, 28 },
    { "Remove",  "button",   "Remove",    "Remove_Click", 354, 152, 110, 28 },
    { "Status",  "label",    "0 items",   NULL,           16, 240, 448, 20 },
};
static const THandler LIST_HANDLERS[] = {
    { "Items_Select", "  wet.find(\"DetailText\").text = wet.find(\"Items\").text\n" },
    { "Add_Click",    "  wet.messagebox(\"Add\", \"Template stub\")\n" },
    { "Remove_Click", "  wet.messagebox(\"Remove\", \"Template stub\")\n" },
};
static const TDef T_LIST = {
    "listdetail", "List and Detail", "A list box beside a detail pane.",
    480, 320, "Browser",
    LIST_CTRLS, (int)(sizeof LIST_CTRLS / sizeof LIST_CTRLS[0]),
    LIST_HANDLERS, (int)(sizeof LIST_HANDLERS / sizeof LIST_HANDLERS[0]),
};

/* --- Settings ---------------------------------------------------------- */
static const TCtrl SETTINGS_CTRLS[] = {
    { "LblName",  "label",   "Name:",   NULL,           24,  28,  80, 20 },
    { "Name",     "textbox", "",        NULL,           120, 24, 260, 24 },
    { "LblTheme", "label",   "Theme:",  NULL,           24,  68,  80, 20 },
    { "Theme",    "textbox", "",        NULL,           120, 64, 260, 24 },
    { "Dark",     "checkbox","Dark",    NULL,           120, 104, 200, 24 },
    { "LblVol",   "label",   "Volume:", NULL,           24,  148, 80, 20 },
    { "Volume",   "slider",  "",        NULL,           120, 144, 200, 24 },
    { "Apply",    "button",  "Apply",   "Apply_Click",  200, 240,  88, 28 },
    { "Cancel",   "button",  "Cancel",  "Cancel_Click", 300, 240,  88, 28 },
};
static const THandler SETTINGS_HANDLERS[] = {
    { "Apply_Click",  "  wet.messagebox(\"Saved\", \"Settings\")\n" },
    { "Cancel_Click", "  wet.messagebox(\"Cancelled\", \"Settings\")\n" },
};
static const TDef T_SETTINGS = {
    "settings", "Settings Page", "Labelled fields with Apply and Cancel.",
    420, 300, "Settings",
    SETTINGS_CTRLS, (int)(sizeof SETTINGS_CTRLS / sizeof SETTINGS_CTRLS[0]),
    SETTINGS_HANDLERS, (int)(sizeof SETTINGS_HANDLERS / sizeof SETTINGS_HANDLERS[0]),
};

/* --- Toolbar workspace ------------------------------------------------- */
static const TCtrl TOOLBAR_CTRLS[] = {
    { "New",    "button", "New",    "New_Click",    16,  16, 88, 28 },
    { "Open",   "button", "Open",   "Open_Click",   112, 16, 88, 28 },
    { "Save",   "button", "Save",   "Save_Click",   208, 16, 88, 28 },
    { "Workspace", "groupbox", "Workspace", NULL,   16,  60, 488, 264 },
};
static const THandler TOOLBAR_HANDLERS[] = {
    { "New_Click",  "  wet.messagebox(\"New\", \"Toolbar\")\n" },
    { "Open_Click", "  wet.messagebox(\"Open\", \"Toolbar\")\n" },
    { "Save_Click", "  wet.messagebox(\"Save\", \"Toolbar\")\n" },
};
static const TDef T_TOOLBAR = {
    "toolbar", "Toolbar Workspace", "A button row over a large workspace.",
    520, 360, "Workbench",
    TOOLBAR_CTRLS, (int)(sizeof TOOLBAR_CTRLS / sizeof TOOLBAR_CTRLS[0]),
    TOOLBAR_HANDLERS, (int)(sizeof TOOLBAR_HANDLERS / sizeof TOOLBAR_HANDLERS[0]),
};

/* --- Input form -------------------------------------------------------- */
static const TCtrl FORM_CTRLS[] = {
    { "LblFirst", "label",  "First:",  NULL,            24,  28, 90, 20 },
    { "First",    "textbox","",        NULL,            120, 24, 240, 24 },
    { "LblLast",  "label",  "Last:",   NULL,            24,  68, 90, 20 },
    { "Last",     "textbox","",        NULL,            120, 64, 240, 24 },
    { "LblEmail", "label",  "Email:",  NULL,            24, 108, 90, 20 },
    { "Email",    "textbox","",        NULL,            120, 104, 240, 24 },
    { "Submit",   "button", "Submit",  "Submit_Click",  240, 160, 120, 28 },
};
static const THandler FORM_HANDLERS[] = {
    { "Submit_Click",
      "  wet.messagebox(\"Hello \" .. (wet.find(\"First\").text or \"\"),\n"
      "                 \"Form\")\n" },
};
static const TDef T_FORM = {
    "inputform", "Input Form", "Three labelled fields and a submit button.",
    400, 220, "Form",
    FORM_CTRLS, (int)(sizeof FORM_CTRLS / sizeof FORM_CTRLS[0]),
    FORM_HANDLERS, (int)(sizeof FORM_HANDLERS / sizeof FORM_HANDLERS[0]),
};

/* --- Image viewer ------------------------------------------------------ */
static const TCtrl IMAGE_CTRLS[] = {
    { "Picture", "image",  "",           NULL,            16,  16, 256, 256 },
    { "ZoomIn",  "button", "Zoom In",    "ZoomIn_Click",  288, 180,  64, 28 },
    { "ZoomOut", "button", "Zoom Out",   "ZoomOut_Click", 360, 180,  64, 28 },
    { "Caption", "label",  "No image",   NULL,            16,  288, 408, 20 },
};
static const THandler IMAGE_HANDLERS[] = {
    { "ZoomIn_Click",  "  wet.messagebox(\"Zoom in\", \"Viewer\")\n" },
    { "ZoomOut_Click", "  wet.messagebox(\"Zoom out\", \"Viewer\")\n" },
};
static const TDef T_IMAGE = {
    "imageviewer", "Image Viewer", "A picture with zoom controls and a caption.",
    440, 340, "Viewer",
    IMAGE_CTRLS, (int)(sizeof IMAGE_CTRLS / sizeof IMAGE_CTRLS[0]),
    IMAGE_HANDLERS, (int)(sizeof IMAGE_HANDLERS / sizeof IMAGE_HANDLERS[0]),
};

/* --- Progress ---------------------------------------------------------- */
static const TCtrl PROGRESS_CTRLS[] = {
    { "Progress", "progress", "",          NULL,           24, 32, 352, 20 },
    { "Status",   "label",    "Idle",      NULL,           24, 72, 352, 20 },
    { "Start",    "button",   "Start",     "Start_Click",  150, 112, 88, 28 },
    { "Cancel",   "button",   "Cancel",    "Cancel_Click", 250, 112, 88, 28 },
};
static const THandler PROGRESS_HANDLERS[] = {
    { "Start_Click",  "  wet.find(\"Status\").text = \"Working...\"\n" },
    { "Cancel_Click", "  wet.find(\"Status\").text = \"Idle\"\n" },
};
static const TDef T_PROGRESS = {
    "progress", "Task with Progress", "A progress bar, a status line, Start and Cancel.",
    400, 180, "Progress",
    PROGRESS_CTRLS, (int)(sizeof PROGRESS_CTRLS / sizeof PROGRESS_CTRLS[0]),
    PROGRESS_HANDLERS, (int)(sizeof PROGRESS_HANDLERS / sizeof PROGRESS_HANDLERS[0]),
};

/* --- Calculator -------------------------------------------------------- */
/*
 * A whole working PROGRAM, not just a layout: a four-function calculator whose
 * buttons feed a display and whose '=' evaluates the expression.  The evaluator
 * lives INSIDE the handler (a nested local) because a template's handler stubs
 * are emitted as separate functions -- there is nowhere else to put a helper.
 *
 * Buttons share one handler and are distinguished by self.text, so adding a
 * key is a one-line change to the control table.
 */
static const TCtrl CALC_CTRLS[] = {
    { "Display", "label", "0",  NULL,  16,  16, 368, 40 },
    /* row 1 */ { "KeyC",   "button", "C", "Key",  16,  72, 72, 44 },
                { "KeyPct", "button", "%", "Key", 112,  72, 72, 44 },
                { "KeyDiv", "button", "/", "Key", 208,  72, 72, 44 },
                { "KeyMul", "button", "*", "Key", 304,  72, 72, 44 },
    /* row 2 */ { "Key7",   "button", "7", "Key",  16, 124, 72, 44 },
                { "Key8",   "button", "8", "Key", 112, 124, 72, 44 },
                { "Key9",   "button", "9", "Key", 208, 124, 72, 44 },
                { "KeySub", "button", "-", "Key", 304, 124, 72, 44 },
    /* row 3 */ { "Key4",   "button", "4", "Key",  16, 176, 72, 44 },
                { "Key5",   "button", "5", "Key", 112, 176, 72, 44 },
                { "Key6",   "button", "6", "Key", 208, 176, 72, 44 },
                { "KeyAdd", "button", "+", "Key", 304, 176, 72, 44 },
    /* row 4 */ { "Key1",   "button", "1", "Key",  16, 228, 72, 44 },
                { "Key2",   "button", "2", "Key", 112, 228, 72, 44 },
                { "Key3",   "button", "3", "Key", 208, 228, 72, 44 },
                { "KeyEq",  "button", "=", "Key", 304, 228, 72, 44 },
    /* row 5 */ { "Key0",   "button", "0", "Key",  16, 280, 72, 44 },
                { "KeyDot", "button", ".", "Key", 112, 280, 72, 44 },
};
static const THandler CALC_HANDLERS[] = {
    { "Key",
      "  local d = wet.find('Display')\n"
      "  local k = self.text\n"
      "  if k == 'C' then d.text = '0'; return end\n"
      "  if k == '%' then d.text = tostring(tonumber(d.text) / 100); return end\n"
      "  if k == '=' then\n"
      "    local nums, ops, cur = {}, {}, ''\n"
      "    for c in d.text:gmatch('.') do\n"
      "      if c == '+' or c == '-' or c == '*' or c == '/' then\n"
      "        nums[#nums + 1] = tonumber(cur); ops[#ops + 1] = c; cur = ''\n"
      "      else cur = cur .. c end\n"
      "    end\n"
      "    nums[#nums + 1] = tonumber(cur)\n"
      "    local n2, o2 = { nums[1] }, {}\n"
      "    for i = 1, #ops do\n"
      "      if ops[i] == '*' then n2[#n2] = n2[#n2] * nums[i + 1]\n"
      "      elseif ops[i] == '/' then n2[#n2] = n2[#n2] / nums[i + 1]\n"
      "      else n2[#n2 + 1] = nums[i + 1]; o2[#o2 + 1] = ops[i] end\n"
      "    end\n"
      "    local acc = n2[1]\n"
      "    for i = 1, #o2 do\n"
      "      if o2[i] == '+' then acc = acc + n2[i + 1] else acc = acc - n2[i + 1] end\n"
      "    end\n"
      "    d.text = tostring(acc)\n"
      "    return\n"
      "  end\n"
      "  d.text = (d.text == '0' and k) or (d.text .. k)\n" },
};
static const TDef T_CALC = {
    "calculator", "Calculator", "A working four-function calculator.",
    400, 340, "Calculator",
    CALC_CTRLS, (int)(sizeof CALC_CTRLS / sizeof CALC_CTRLS[0]),
    CALC_HANDLERS, (int)(sizeof CALC_HANDLERS / sizeof CALC_HANDLERS[0]),
};

/* --- the library, in picker order -------------------------------------- */
static const TDef *const TEMPLATES[] = {
    &T_BLANK, &T_DIALOG, &T_LIST, &T_SETTINGS,
    &T_TOOLBAR, &T_FORM, &T_IMAGE, &T_PROGRESS, &T_CALC,
};

#define TEMPLATE_COUNT ((int)(sizeof TEMPLATES / sizeof TEMPLATES[0]))

static const TDef *find(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < TEMPLATE_COUNT; i++)
        if (strcmp(TEMPLATES[i]->name, name) == 0) return TEMPLATES[i];
    return NULL;
}

/* ------------------------------------------------------------- accessors */

int                vws_template_count(void) { return TEMPLATE_COUNT; }

const VwsTemplate *vws_template_at(int i) {
    if (i < 0 || i >= TEMPLATE_COUNT) return NULL;
    return (const VwsTemplate *)TEMPLATES[i];   /* name/label/description */
}

const VwsTemplate *vws_template_find(const char *name) {
    const TDef *t = find(name);
    return t ? (const VwsTemplate *)t : NULL;
}

const char *vws_template_label(const char *name) {
    const TDef *t = find(name);
    return t ? t->label : NULL;
}

const char *vws_template_description(const char *name) {
    const TDef *t = find(name);
    return t ? t->description : NULL;
}

const char *vws_template_title(const char *name) {
    const TDef *t = find(name);
    return t ? t->title : NULL;
}

void vws_template_foreach_ctrl(const char *name, VwsTemplateCtrlFn fn, void *ud) {
    const TDef *t = find(name);
    if (!t || !fn) return;
    for (int i = 0; i < t->nctrls; i++) {
        const TCtrl *c = &t->ctrls[i];
        fn(ud, c->id, c->cls, c->text, c->handler,
           c->left, c->top, c->w, c->h);
    }
}

/* --------------------------------------------------------------- emitter */

typedef struct { char *data; size_t len, cap; } Buf;

static bool reserve(Buf *b, size_t extra) {
    if (b->len + extra + 1 <= b->cap) return true;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->len + extra + 1) cap *= 2;
    char *grown = (char *)realloc(b->data, cap);
    if (!grown) return false;
    b->data = grown;
    b->cap = cap;
    return true;
}

static bool put(Buf *b, const char *s) {
    size_t n = strlen(s);
    if (!reserve(b, n)) return false;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    return true;
}

static bool putf(Buf *b, const char *fmt, ...) {
    va_list ap;

    /* First pass: how long is this piece?  vsnprintf(NULL, 0, ...) is C99 and
     * returns the length without writing. */
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n <= 0) return n == 0;   /* an empty piece is a success, not a failure */

    /* A single piece can be long -- a big handler body, a wide control table.
     * reserve() guarantees n bytes plus the NUL, so the second pass writes
     * straight into the destination.  (The old fixed 512-byte stack buffer
     * silently truncated anything longer.) */
    if (!reserve(b, (size_t)n)) return false;

    va_start(ap, fmt);
    vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap);
    va_end(ap);
    b->len += (size_t)n;
    return true;
}

/* A Lua double-quoted literal. */
static bool put_lit(Buf *b, const char *s) {
    static const char hex[] = "0123456789abcdef";
    if (!put(b, "\"")) return false;
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        bool ok = true;
        switch (*p) {
        case '"':  ok = put(b, "\\\""); break;
        case '\\': ok = put(b, "\\\\"); break;
        case '\n': ok = put(b, "\\n");  break;
        case '\r': ok = put(b, "\\r");  break;
        case '\t': ok = put(b, "\\t");  break;
        default:
            if (*p < 0x20) ok = putf(b, "\\x%c%c", hex[*p >> 4], hex[*p & 0xF]);
            else           ok = putf(b, "%c", (char)*p);
        }
        if (!ok) return false;
    }
    return put(b, "\"");
}

char *vws_template_emit(const char *name) {
    const TDef *t = find(name);
    if (!t) return NULL;

    Buf b = {0};
    bool ok = true;

    ok = ok && putf(&b,
        "-- %s.vws -- Visual WuBu System\n"
        "-- Created from the \"%s\" template.  Edit here or in VWS; the\n"
        "-- handler bodies below are preserved verbatim across re-emits.\n\n",
        t->title, t->label);

    /* Handler stubs first, exactly as a designer re-emit would. */
    for (int i = 0; i < t->nhandlers && ok; i++) {
        ok = putf(&b, "function %s(self)\n%s\nend\n\n",
                  t->handlers[i].name,
                  t->handlers[i].body ? t->handlers[i].body : "");
    }

    ok = ok && putf(&b, "wet.applet {\n  title = ");
    ok = ok && put_lit(&b, t->title);
    ok = ok && putf(&b, ",\n  width = %d, height = %d,\n", t->def_w, t->def_h);
    ok = ok && putf(&b, "  content = wet.canvas {\n");

    if (t->nctrls == 0) {
        ok = ok && put(&b, "    children = {},\n");
    } else {
        ok = ok && put(&b, "    children = {\n");
        for (int i = 0; i < t->nctrls && ok; i++) {
            const TCtrl *c = &t->ctrls[i];
            ok = ok && putf(&b, "      wet.%s { id=", c->cls);
            ok = ok && put_lit(&b, c->id);
            ok = ok && putf(&b, ", left=%d, top=%d, w=%d, h=%d",
                            c->left, c->top, c->w, c->h);
            if (c->text && c->text[0]) {
                ok = ok && put(&b, ", text=");
                ok = ok && put_lit(&b, c->text);
            }
            if (c->handler && c->handler[0]) {
                ok = ok && putf(&b, ", onclick=%s", c->handler);
            }
            ok = ok && put(&b, " },\n");
        }
        ok = ok && put(&b, "    },\n");
    }

    ok = ok && put(&b, "  },\n}\n");

    if (!ok) { free(b.data); return NULL; }
    if (!reserve(&b, 0)) { free(b.data); return NULL; }
    b.data[b.len] = '\0';
    return b.data;
}
