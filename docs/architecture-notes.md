# Architecture Notes

## Product Shape

`npp-markdown-features` should be a single Notepad++ plugin that collects Markdown-focused editing and viewing improvements behind one menu and one settings surface.

The first feature is a rendered/raw display toggle for Markdown files. Future features should be added as capabilities inside the same plugin unless they require a fundamentally separate lifecycle or dependency model.

## Notepad++ Integration Model

Expected integration points:

- Native C++ Notepad++ plugin DLL
- Plugin menu commands under `Plugins > Markdown Features`
- Toolbar button for the primary rendered/raw toggle
- Optional keyboard shortcut via Notepad++ shortcut mapper
- Notepad++ and Scintilla notifications for buffer activation, file saves, and document changes

## Rendering Model

Rendered mode should not mutate the active document buffer. The plugin should render the current Markdown source into a separate view layered into the editor area.

The likely first implementation is:

- Keep Scintilla as the raw source editor.
- Create a WebView2 child window for rendered HTML.
- Show/hide the WebView2 view when toggling modes.
- Re-render from the Scintilla buffer on save or live text changes, depending on settings.

## Open Questions

- Whether rendered/raw mode should be global across all Markdown buffers or remembered per buffer by default.
- Whether WebView2 should be a hard dependency or optional with a fallback message.
- Which Markdown parser should be used first: `md4c`, `cmark-gfm`, or another small native library.
- How much CSS customization should be exposed in the first version.
