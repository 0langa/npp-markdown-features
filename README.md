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

Version `0.1.0` builds a native x64 Notepad++ plugin DLL with:

- `Plugins > Markdown Features` menu
- rendered/raw Markdown toggle
- manual refresh command
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
