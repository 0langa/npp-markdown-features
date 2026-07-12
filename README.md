# npp-markdown-features

[![Latest release](https://img.shields.io/github/v/release/0langa/npp-markdown-features?sort=semver)](https://github.com/0langa/npp-markdown-features/releases/latest)
[![Windows Build](https://github.com/0langa/npp-markdown-features/actions/workflows/windows-build.yml/badge.svg)](https://github.com/0langa/npp-markdown-features/actions/workflows/windows-build.yml)
[![License: MIT](https://img.shields.io/github/license/0langa/npp-markdown-features)](LICENSE)
![Platform: Windows x64](https://img.shields.io/badge/platform-Windows%20x64-blue)

An all-in-one Markdown plugin for Notepad++ — rendering, navigation, editing assistance, tables, links, export, and cleanup in a single native plugin with one menu and one settings file.

Version **1.0.0** · Windows x64 · MIT licensed · requires the Microsoft Edge WebView2 Runtime (preinstalled on Windows 11).

## The ten features

1. **Rendered Markdown view** — toggle between raw editing and a rendered GitHub-flavored view right in the editor area (toolbar button, menu, `Ctrl+Shift+M`). Tables, task lists, strikethrough, autolinks, footnotes. Block-level scroll sync keeps your reading position exact in both directions (word-wrap aware, `data-sourcepos` mapping with heading/ratio fallbacks). External links open in your browser; page scripts are always disabled, so a Markdown file can never run JavaScript.
2. **Document Outline panel** (`Ctrl+Shift+O`) — docked heading tree with live updates while you type, a filter box, and click-to-jump that also scrolls the rendered view. ATX and setext headings; visibility persists across restarts; follows the Notepad++ dark mode.
3. **Smart list editing** — Enter continues bullets, numbered lists (incl. zero-padded and `1)` styles), task items, and blockquotes; Enter on an empty item ends the list; ordered lists renumber automatically on insertion; `Renumber List` fixes any block on demand; `Toggle Task Checkbox` (`Ctrl+Shift+X`) checks/unchecks/converts, selections included.
4. **Table tools** — `Format Table` (`Ctrl+Shift+T`) pretty-prints with alignment-aware padding (escaped pipes and UTF-8 widths handled) and scaffolds new tables from a lone header line; Tab/Shift+Tab hop between cells with reformatting on the way; Tab in the last cell appends a row; insert/delete columns; cycle column alignment.
5. **Formatting commands** — Bold/Italic/Strikethrough/Inline Code toggles (`Ctrl+Alt+B/I/U/C`) that operate on the selection or word at the caret and unwrap when already applied; heading level up/down; blockquote toggle (`Ctrl+Alt+Q`); code fence insertion (`Ctrl+Alt+F`). The plugin menu is grouped into Format / Lists / Table / Links / Export submenus.
6. **Link & image tooling** — paste a URL over selected text to create `[text](url)`; Insert Link (`Ctrl+Alt+L`) / Insert Image scaffolds; Follow Link (`Ctrl+Alt+G`) opens URLs in the browser, jumps to `#anchors`, and opens relative file links in Notepad++ (reference links and `%20` decoding included); Check Links reports every broken local file link or anchor with line numbers.
7. **Export & copy** — Export HTML writes a standalone styled document; Copy as HTML puts real `HTML Format` data on the clipboard for rich pasting into Word/Outlook/mail; Print Rendered View opens the WebView2 print dialog (print or save as PDF).
8. **TOC & document cleanup** — Insert/Update TOC maintains a marker-delimited table of contents with GitHub-style slugs; Format Document normalizes trailing whitespace (hard breaks preserved), blank lines, bullet markers, spacing around headings/fences, and reformats every table — never touching fenced code. Both idempotent, single-undo.
9. **Live stats & breadcrumb** — word count, characters, reading time, and the current heading path in the status bar as you type and move; selection-aware; YAML frontmatter and code excluded from counts. Frontmatter renders as a tidy collapsible block instead of broken markup.
10. **Rendered-view polish** — syntax highlighting in code blocks via an embedded highlight.js (injected host-side; page scripts stay disabled), light/dark/auto themes, an optional custom CSS file, and Ctrl+wheel zoom that persists across sessions.

All editing assists apply only to Markdown files (configurable extensions), group as single undo actions, and can be switched off via the `Smart Typing` menu toggle.

## Install

1. Close Notepad++.
2. Download the latest release zip from the [**Releases** page](https://github.com/0langa/npp-markdown-features/releases/latest) and copy `plugins/NppMarkdownFeatures/NppMarkdownFeatures.dll` into `C:\Program Files\Notepad++\plugins\NppMarkdownFeatures\`.
3. Start Notepad++ — you'll find everything under `Plugins > Markdown Features`.

WebView2 profile data is stored under `%LOCALAPPDATA%\NppMarkdownFeatures\WebView2`; settings live in the Notepad++ plugin config directory as `NppMarkdownFeatures\settings.json`.

## Settings

`Plugins > Markdown Features > Settings...` covers default display mode, Markdown extensions, preview theme (auto/light/dark), and a custom CSS file. The JSON settings file additionally holds fine-grained switches (`listEditing.smartEnter`, `tableEditing.smartTab`, `linkTools.pasteUrlAsLink`, `markdownView.zoom`).

## Build

Requirements: Windows x64, CMake, Ninja, NuGet CLI, Visual Studio C++ x64 build tools, WebView2 Runtime.

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release     # build + unit tests
powershell -ExecutionPolicy Bypass -File .\scripts\package.ps1 -Configuration Release   # release zip in dist\
powershell -ExecutionPolicy Bypass -File .\scripts\install-local.ps1 -Configuration Release
```

Dependencies are pinned and fetched at configure time: [cmark-gfm](https://github.com/github/cmark-gfm) (parsing/rendering), [nlohmann/json](https://github.com/nlohmann/json) (settings), the WebView2 SDK (NuGet), and a vendored [highlight.js](https://github.com/highlightjs/highlight.js) 11.9.0 (`third_party/highlight`, BSD-3-Clause).

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for the full version history (v0.1.0 → v1.0.0).

## License

MIT (plugin). Bundled highlight.js is BSD-3-Clause; cmark-gfm is MIT/BSD-style — see their license files.
