# npp-markdown-features — Status & Roadmap
_Portfolio audit: 2026-07-11. State refreshed: 2026-09-19._

## What this is
An all-in-one native Markdown plugin for Notepad++ (Windows x64, C++): rendered
GitHub-flavored preview with block-level scroll sync (cmark-gfm + WebView2), document
outline panel, smart list editing, table tools, formatting commands, link/image tooling,
HTML export and rich clipboard copy, TOC generation, document cleanup, live stats, and
themable rendering with vendored highlight.js. Built with CMake/Ninja, unit-tested
(`tests/main.cpp`, ~700 lines against the `nmf_core` library), with a GitHub Actions
Windows build + package workflow and PowerShell build/package/install scripts.

## Current state
The healthiest project in the portfolio. 21 well-scoped commits over 2026-07-05/12,
clean working tree, and a disciplined tag ladder `v0.1.0` through `v1.0.0` (all tagged
2026-07-06). The v1.0.0 GitHub release is public and carries
`NppMarkdownFeatures-v1.0.0-win-x64.zip`. The README is release-quality and documents all ten
features, install steps, settings, and third-party licenses. `CHANGELOG.md` covers the full
v0.1.0 to v1.0.0 history. `scripts/package.ps1` derives the version from `CMakeLists.txt`
(`project(NppMarkdownFeatures VERSION 1.0.0 ...)`), and the CI workflow uploads the wildcard
`dist/NppMarkdownFeatures-v*-win-x64.zip` path.

Loose ends found:

- Not yet submitted to the Notepad++ Plugins Admin list, which is where plugins get
  discovered and installed by real users.
- The sibling folder `..\npp-markdown-view` is empty (created 2026-07-05, the same day
  this repo started). It appears to be the abandoned original working folder for the
  "Markdown View" idea before it grew into this all-in-one plugin — safe to delete.

## Definition of "finished"
v1.0.0 is essentially finished. Wrap-up means submission to the Notepad++ Plugins Admin list,
which gives it a life beyond this machine. After that,
the project moves to maintenance mode — respond to issues, bump pinned deps
occasionally.

## Roadmap

### Phase 1 — Now (next 1-2 weeks)
1. Install the published v1.0.0 zip into a clean Notepad++ profile and record one smoke-test pass.
2. Submit the plugin to the Notepad++ Plugins Admin (`nppPluginList`) with the stable release URL
   and DLL hash.

### Phase 2 — Next (2-6 weeks)
1. Add a release-tag-triggered workflow so future tags publish automatically.
2. Light dogfooding pass on large documents (multi-MB Markdown) to confirm scroll-sync
   and outline performance claims.

### Phase 3 — Later (optional/stretch)
- ARM64 build if Notepad++ ARM users ask for it.
- Optional features parked in `docs/architecture-notes.md` (e.g. Mermaid/diagram
  rendering) only if user demand appears.
- Consider a small demo GIF in the README for the plugin-list listing.

## Effort to "finished"
**S** — one clean-profile smoke test and one plugin-list submission; the engineering is done.
