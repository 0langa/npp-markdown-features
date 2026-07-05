# npp-markdown-features

A Notepad++ plugin project for improving the Markdown authoring experience in Notepad++.

The first goal is a program-wide Markdown display toggle: when editing Markdown files, the user should be able to switch between the normal raw text editor and a rendered Markdown view from a menu item, toolbar button, and shortcut. The rendered view should use the main editor area rather than a permanent split view or docked preview panel.

## Project Direction

This repository is intended to grow into a bundled Markdown utility plugin instead of a collection of separate single-purpose plugins. The first version should establish the plugin shell, settings structure, and UI conventions so future Markdown-specific features have a clear home.

## First Version Plan

1. Scaffold a native Notepad++ plugin from the C++ plugin template.
2. Register a `Markdown Features` plugin menu with:
   - `Toggle Rendered View`
   - `Settings`
   - `About`
3. Add a toolbar button for the rendered/raw Markdown toggle.
4. Detect Markdown buffers by file extension and current language where possible.
5. Implement raw mode by leaving the normal Scintilla editor visible and editable.
6. Implement rendered mode as a read-only WebView2 overlay in the main editor area.
7. Render Markdown through a small, well-maintained parser such as `md4c` or `cmark-gfm`.
8. Keep the source buffer untouched; rendered mode must never replace document text with generated HTML.
9. Persist user settings for default mode, supported extensions, and live refresh behavior.
10. Add a repeatable build and install path for local Notepad++ testing.

## Initial Settings Shape

The first settings dialog should stay minimal:

- `Default Markdown display`: raw text or rendered
- `Toggle behavior`: per-buffer or global
- `Live preview refresh`: on save or while typing
- `Markdown file extensions`: configurable list, defaulting to `.md` and `.markdown`
- `Renderer`: internal parser initially, with room for future options

## Development Status

Planning and repository setup are in progress. No plugin binary exists yet.

## License

MIT
