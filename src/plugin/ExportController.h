#pragma once

#include "rendering/MarkdownRenderer.h"

#include <string>
#include <windows.h>

namespace nmf {

// Export commands: standalone HTML file, CF_HTML clipboard copy.
class ExportController {
public:
    // Renders the document and writes a standalone HTML file chosen via a
    // save dialog (default: next to the source, .html). Returns a status
    // message.
    std::wstring ExportHtml(HWND scintilla, HWND npp, const std::wstring& documentPath, const std::string& markdownUtf8);

    // Renders the selection (or whole document) and puts it on the clipboard
    // as both "HTML Format" (rich paste into Word/Outlook/mail) and plain
    // text (the raw HTML). Returns a status message.
    std::wstring CopyAsHtml(HWND scintilla, const std::wstring& documentPath, const std::string& markdownUtf8);

private:
    MarkdownRenderer renderer_{};
};

}  // namespace nmf
