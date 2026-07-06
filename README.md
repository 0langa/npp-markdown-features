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

Version `0.7.0` builds a native x64 Notepad++ plugin DLL with:

- `Plugins > Markdown Features` menu
- rendered/raw Markdown toggle (toolbar button, menu, `Ctrl+Shift+M`)
- GitHub-flavored rendering via `cmark-gfm` (tables, task lists, strikethrough, autolinks, footnotes)
- block-level scroll sync: toggling raw/rendered keeps your reading position both ways, using `data-sourcepos` source mapping with heading-anchor and ratio fallbacks
- word-wrap-aware scroll mapping in the raw editor
- **Document Outline** panel (`Ctrl+Shift+O`): docked heading tree with live updates while typing, filter box, and click-to-jump that also scrolls the rendered view; panel visibility persists across restarts; ATX and setext headings supported
- **Smart list editing** (toggleable via `Plugins > Markdown Features > Smart List Editing`):
  - Enter continues bullet lists (`-`, `*`, `+`), ordered lists (`1.`, `1)`, zero-padded numbers), task lists, and blockquotes — indentation preserved, remainder of a split line carried over
  - Enter on an empty item removes the marker and ends the list
  - ordered lists renumber automatically when items are inserted; `Renumber List` fixes any block on demand (nesting-aware, keeps the starting number)
  - `Toggle Task Checkbox` (`Ctrl+Shift+X`): checks/unchecks tasks, adds boxes to plain list items, converts plain lines or selections into task items
- **Table tools**:
  - `Format Table` (`Ctrl+Shift+T`): pretty-prints the pipe table under the caret (alignment-aware padding, escaped `\|` respected, UTF-8 cell widths); on a lone `| Header |` line it scaffolds the delimiter row and an empty body row
  - Tab / Shift+Tab move between cells (reformatting as you go); Tab in the last cell appends a new row — works when Notepad++ inserts tab characters (default)
  - `Table: Insert Column` / `Table: Delete Column` at the caret position
  - `Table: Cycle Column Alignment` steps the current column through none → left → center → right
  - smart Tab is part of the `Smart Typing (Lists, Tables)` toggle
- **Formatting commands** (under `Markdown Features > Format`; the plugin menu is organized into `Format` / `Lists` / `Table` submenus):
  - Bold (`Ctrl+Alt+B`), Italic (`Ctrl+Alt+I`), Strikethrough (`Ctrl+Alt+U`), Inline Code (`Ctrl+Alt+C`) — toggle on the selection or the word at the caret, unwrap when already wrapped, bold/italic edges disambiguated
  - Heading Level Up / Down for the current line or all selected lines
  - Toggle Blockquote (`Ctrl+Alt+Q`) adds/removes one `> ` level across the selection
  - Insert Code Fence (`Ctrl+Alt+F`) wraps the selected lines in ``` fences or inserts an empty block, caret on the language position
- **Link & image tooling** (`Markdown Features > Links`):
  - paste a URL over selected text (`Ctrl+V`) and it becomes `[selection](url)` automatically (part of Smart Typing)
  - Insert Link (`Ctrl+Alt+L`) / Insert Image scaffolds with the caret in the right slot
  - Follow Link (`Ctrl+Alt+G`): opens `https://` links in the browser, `#anchors` jump to the matching heading, relative file links open in Notepad++ (with `%20` decoding and `[ref][id]` resolution)
  - Check Links: validates every local file link and heading anchor in the document and opens a report listing broken ones (external URLs are counted but not fetched)
- **Export** (`Markdown Features > Export`):
  - `Export HTML...` writes a standalone styled HTML file (save dialog, inline CSS, relative images resolve via `<base>`)
  - `Copy as HTML` puts the rendered selection (or whole document) on the clipboard as `HTML Format` — paste rich text straight into Word, Outlook, or Gmail — plus the raw HTML as plain text
  - `Print Rendered View...` opens the WebView2 print dialog (print or save as PDF); renders first if you're in raw mode
- minimal persisted settings dialog
- WebView2 rendered Markdown overlay
- core unit tests

WebView2 profile data is stored under the current user's local app data, not under the Notepad++ installation directory:

```text
%LOCALAPPDATA%\NppMarkdownFeatures\WebView2
```

## Build

Requirements:

- Windows x64
- CMake
- Ninja
- NuGet CLI
- Visual Studio C++ x64 build tools
- Microsoft Edge WebView2 Runtime

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release
```

Build output:

```text
build\bin\Release\NppMarkdownFeatures.dll
```

## Package

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package.ps1 -Configuration Release
```

Package output:

```text
dist\NppMarkdownFeatures-v0.1.0-win-x64.zip
```

## Install Locally

Close Notepad++ first.

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install-local.ps1 -Configuration Release
```

If the shell is not elevated, Program Files will require admin rights. Run this from an elevated PowerShell if the script cannot verify the copy:

```powershell
$ErrorActionPreference='Stop'; New-Item -ItemType Directory -Force -Path "C:\Program Files\Notepad++\plugins\NppMarkdownFeatures" | Out-Null; Copy-Item -LiteralPath "C:\Users\Julius\source\repos\npp-markdown-features\build\bin\Release\NppMarkdownFeatures.dll" -Destination "C:\Program Files\Notepad++\plugins\NppMarkdownFeatures\NppMarkdownFeatures.dll" -Force
```

Expected installed DLL:

```text
C:\Program Files\Notepad++\plugins\NppMarkdownFeatures\NppMarkdownFeatures.dll
```

## Manual Verification

1. Close Notepad++.
2. Build Release.
3. Install the DLL into `C:\Program Files\Notepad++\plugins\NppMarkdownFeatures`.
4. Start Notepad++ normally.
5. Confirm `Plugins > Markdown Features` exists.
6. Confirm the toolbar icon appears.
7. Open or create `test.md` with headings, lists, task lists, table, fenced code, link, and local image.
8. Use `Plugins > Markdown Features > Toggle Rendered View`; the editor area should switch to rendered Markdown without a docked or split panel.
9. Toggle again; raw Markdown text should be unchanged and editable.
10. Edit Markdown, save, then use `Refresh Rendered View`; rendered output should update.
11. Open a `.txt` file; rendered overlay should disappear.
12. Switch back to `.md`; rendered mode should apply again if global rendered mode is on.
13. Open Settings, change default mode or extensions, restart Notepad++, confirm persistence.
14. Click an external link in rendered view; default browser should open.
15. Restart Notepad++; no startup error or orphan overlay should appear.

## License

MIT
