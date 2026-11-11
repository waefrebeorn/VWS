# VWS — Visual WuBu System

**WuBu's Visual Basic.** A form designer and code-behind editor over the
[.WET](https://github.com/waefrebeorn/WET) applet framework.

VB's real contribution was never the language — it was the **loop**: drag a
control onto a form, double-click it, write the handler, F5. That is what VWS
slerms. The language is Lua (already the .WET surface); VWS adds the designer.


> **~WuBu~ AGI** — one system, many repositories. This is the **forms designer**. The Body is [WuBuOS](https://github.com/waefrebeorn/WuBuOS), the Brain wubuwizard, the Compiler WuBuNOS.


> **~WuBu~ AGI** — one system, many repositories. This is the **forms designer**. The Body is [WuBuOS](https://github.com/waefrebeorn/WuBuOS), the Brain wubuwizard, the Compiler WuBuNOS.
## The file model

A `.vws` file **is** a `.wet` Lua script. One representation, runnable and
editable — VB's `.frm` model.

```lua
function Button1_Click(self)
  wet.messagebox('hi', 'Form1')
end

wet.applet {
  title = 'Form1',
  width = 400, height = 300,
  content = wet.canvas {
    children = {
      wet.button { id='Button1', left=24, top=40, w=100, h=28, text='OK' },
    }
  }
}
```

Because the file is Lua, **the designer's view and the running app cannot
drift**: loading runs the file through .WET and walks the built widget tree;
saving emits Lua. There is no separate `.vrx` to deserialise.

Handler bodies are recovered from the source and re-emitted **verbatim**, so
saving from the designer never clobbers user code. That is the `.frm` guarantee
and it is enforced by a test.

## Layout

Position is absolute, which is VB's model: a form emits a `wet.canvas` with
`left`/`top` and an explicit `w`/`h` on each child. The whole form reflows as a
group; individual controls stay where the designer put them.

## What is here

| File | Owns |
|---|---|
| `vws_form.c` | the document model and the designer edits (add, move, resize, remove, z-order, hit-test, palette) |
| `vws_emit.c` | turning a document into Lua |
| `vws_load.c` | running a file through .WET and walking the tree back into a document |
| `vws_selftest.c` | the round trip and the designer edits |

## Building

```bash
git submodule update --init --recursive   # fetches WET, which fetches Wua
make test
```

54 checks: the document model, hit-testing and z-order, the palette, the
emitter (including Lua string escaping), the **round trip**, handler-body
preservation, and the failure paths.

## Status

This is the document model, the emitter and the loader — the round trip is
proven. The interactive designer surface (drag/drop, the properties grid, the
event picker) is the next slice; it consumes exactly this API.
