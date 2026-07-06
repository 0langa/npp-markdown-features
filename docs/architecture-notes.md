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

## Parser and Scroll Sync

The renderer uses `cmark-gfm` (GitHub's CommonMark fork) with the `table`,
`strikethrough`, `tasklist`, and `autolink` extensions plus footnotes.
`CMARK_OPT_SOURCEPOS` stamps every block element with
`data-sourcepos="startline:startcol-endline:endcol"` (1-based lines), which is
the backbone of raw/rendered scroll preservation:

- Raw -> rendered: the first visible document line (word-wrap aware via
  `SCI_DOCLINEFROMVISIBLE`) becomes a fractional 1-based source line. Script in
  the WebView finds the innermost block whose range starts at or before that
  line and scrolls to an interpolated offset inside it.
- Rendered -> raw: a 250 ms capture timer records the innermost block crossing
  the viewport top and interpolates a fractional source line from its range.
  Toggling back positions the raw editor at that document line via
  `SCI_VISIBLEFROMDOCLINE`.
- Fallback order when sourcepos data is unavailable: nearest heading anchor id,
  then plain viewport ratio.

`MarkdownOutline` extracts headings (ATX and setext) from the cmark-gfm AST —
the same parser configuration as the renderer, so heading lines always agree
with `data-sourcepos`. Stable heading ids (`nmf-heading-*`) are injected into
the rendered HTML and serve as the anchor fallback channel.

## Document Outline Panel

`OutlinePanel` (src/ui) is a docking-manager panel (right side by default)
holding a filter edit box and a heading tree:

- Registered lazily on first show via `NPPM_DMMREGASDCKDLG`; visibility is
  persisted in settings and restored on startup.
- Live updates: `SCN_MODIFIED` (insert/delete) debounces a 350 ms timer that
  re-parses the outline; unchanged outlines skip the tree rebuild.
- Click or Enter jumps to the heading's document line; in rendered mode the
  WebView overlay is scrolled to the same source line instead of focusing the
  editor.
- Dark mode is handled by `NPPM_DARKMODESUBCLASSANDTHEME` on creation and on
  `NPPN_DARKMODECHANGED`.

## Open Questions

- Whether rendered/raw mode should be global across all Markdown buffers or remembered per buffer by default.
- Whether WebView2 should be a hard dependency or optional with a fallback message.
- How much CSS customization should be exposed in the first version.
