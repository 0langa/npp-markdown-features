#pragma once

#include <string>
#include <windows.h>

namespace nmf {

// Scintilla/NPP glue for core/LinkTools: insert link/image scaffolds, follow
// the link at the caret, paste-URL-over-selection, and a local link checker.
class LinkController {
public:
    void SetPasteUrlEnabled(bool enabled);
    bool PasteUrlEnabled() const;

    // Insert [text](url) or ![alt](url) around the selection; caret lands in
    // the first empty slot.
    void InsertLink(HWND scintilla, bool image);

    // Open the link at the caret: external -> browser/mail client, anchor ->
    // jump to the heading, local file -> open in Notepad++. Returns a status
    // bar message.
    std::wstring FollowLink(HWND scintilla, HWND npp, const std::wstring& documentPath);

    // Validate all local links/anchors in the document. status is always
    // set; reportUtf8 is non-empty when issues were found (caller shows it).
    struct CheckResult {
        std::wstring status;
        std::string reportUtf8;
    };
    CheckResult CheckLinks(HWND scintilla, const std::wstring& documentPath);

    // Ctrl+V with a URL on the clipboard and a text selection -> [sel](url).
    // Returns true when the paste was converted (caller swallows the key).
    bool HandlePaste(HWND scintilla);

private:
    bool pasteUrlEnabled_{true};
};

}  // namespace nmf
