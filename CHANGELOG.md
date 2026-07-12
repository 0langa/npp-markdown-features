# Changelog

All notable changes to this project are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

The 1.0.0 release completes the ten planned features, each shipped and verified as its
own release. Every version was unit-tested and live-verified against a real Notepad++
instance before tagging.

## [1.0.0] — 2026-07-06

Rendered-view polish — the tenth and final planned feature.

- **Syntax highlighting** in fenced code blocks via an embedded highlight.js (11.9.0,
  vendored, no CDN or network). It is injected host-side, so page scripts stay disabled
  and Markdown content can never run its own JavaScript.
- **Themes** — auto (follow system), light, or dark, selectable in Settings; highlight
  colors follow the theme.
- **Custom CSS** — point Settings at your own stylesheet; it applies to both the rendered
  view and HTML exports.
- **Zoom persistence** — Ctrl+wheel zoom in the preview is remembered across sessions.

## [0.9.0] — 2026-07-06

Live stats, heading breadcrumb, and frontmatter awareness.

- Word count, character count, and reading time in the status bar, updating as you type
  and move; selection-aware. YAML frontmatter and code are excluded from counts.
- Current heading path shown as a breadcrumb.
- YAML frontmatter renders as a tidy collapsible block instead of broken markup.

## [0.8.0] — 2026-07-06

TOC generation and document cleanup.

- Insert/Update TOC maintains a marker-delimited table of contents with GitHub-style slugs.
- Format Document normalizes trailing whitespace (hard breaks preserved), blank lines,
  bullet markers, spacing around headings and fences, and reformats every table — never
  touching fenced code. Idempotent, single-undo.

## [0.7.0] — 2026-07-06

Export and copy as HTML.

- Export HTML writes a standalone, styled document.
- Copy as HTML places real `HTML Format` (CF_HTML) data on the clipboard for rich pasting
  into Word, Outlook, and mail.
- Print Rendered View opens the WebView2 print dialog (print or save as PDF).

## [0.6.0] — 2026-07-06

Link and image tooling.

- Paste a URL over selected text to create `[text](url)`.
- Insert Link (`Ctrl+Alt+L`) and Insert Image scaffolds.
- Follow Link (`Ctrl+Alt+G`) opens URLs in the browser, jumps to `#anchors`, and opens
  relative file links in Notepad++ (reference links and `%20` decoding included).
- Check Links reports every broken local file link or anchor with line numbers.

## [0.5.0] — 2026-07-06

Formatting commands and grouped menus.

- Bold / Italic / Strikethrough / Inline Code toggles (`Ctrl+Alt+B/I/U/C`) that operate
  on the selection or the word at the caret and unwrap when already applied.
- Heading level up/down, blockquote toggle (`Ctrl+Alt+Q`), and code fence insertion
  (`Ctrl+Alt+F`).
- The plugin menu is grouped into Format / Lists / Table / Links / Export submenus.

## [0.4.0] — 2026-07-06

Table tools.

- Format Table (`Ctrl+Shift+T`) pretty-prints with alignment-aware padding (escaped pipes
  and UTF-8 widths handled) and scaffolds a new table from a lone header line.
- Tab / Shift+Tab hop between cells with reformatting; Tab in the last cell appends a row.
- Insert/delete columns and cycle column alignment.

## [0.3.0] — 2026-07-06

Smart list and checkbox editing.

- Enter continues bullets, numbered lists (including zero-padded and `1)` styles), task
  items, and blockquotes; Enter on an empty item ends the list.
- Ordered lists renumber automatically on insertion; Renumber List fixes any block on demand.
- Toggle Task Checkbox (`Ctrl+Shift+X`) checks / unchecks / converts, selections included.

## [0.2.0] — 2026-07-06

Document Outline panel (`Ctrl+Shift+O`).

- Docked heading tree with live updates while you type, a filter box, and click-to-jump
  that also scrolls the rendered view.
- ATX and setext headings; visibility persists across restarts; follows Notepad++ dark mode.

## [0.1.0] — 2026-07-06

Rendered Markdown view with block-level scroll sync (`Ctrl+Shift+M`).

- Toggle between raw editing and a rendered GitHub-flavored view in the editor area
  (toolbar button, menu, shortcut). Tables, task lists, strikethrough, autolinks, footnotes.
- Block-level scroll sync keeps reading position exact in both directions (word-wrap aware,
  `data-sourcepos` mapping with heading/ratio fallbacks).
- External links open in the browser; page scripts are always disabled, so a Markdown file
  can never run JavaScript.

[1.0.0]: https://github.com/0langa/npp-markdown-features/releases/tag/v1.0.0
[0.9.0]: https://github.com/0langa/npp-markdown-features/releases/tag/v0.9.0
[0.8.0]: https://github.com/0langa/npp-markdown-features/releases/tag/v0.8.0
[0.7.0]: https://github.com/0langa/npp-markdown-features/releases/tag/v0.7.0
[0.6.0]: https://github.com/0langa/npp-markdown-features/releases/tag/v0.6.0
[0.5.0]: https://github.com/0langa/npp-markdown-features/releases/tag/v0.5.0
[0.4.0]: https://github.com/0langa/npp-markdown-features/releases/tag/v0.4.0
[0.3.0]: https://github.com/0langa/npp-markdown-features/releases/tag/v0.3.0
[0.2.0]: https://github.com/0langa/npp-markdown-features/releases/tag/v0.2.0
[0.1.0]: https://github.com/0langa/npp-markdown-features/releases/tag/v0.1.0
