#pragma once

#include <string>
#include <windows.h>

namespace nmf {

// Scintilla glue for the inline/blockwise formatting commands. All edits are
// single undo actions; callers gate to Markdown documents.
class FormattingController {
public:
    // Wrap/unwrap an inline marker around the selection or the word at the
    // caret. Restores a selection over the affected content.
    void ToggleInlineMarker(HWND scintilla, const std::string& marker);

    // Change the ATX heading level of every selected line by delta (+1/-1).
    void ChangeHeading(HWND scintilla, int delta);

    // Add or remove one blockquote level on the selected lines.
    void ToggleBlockquote(HWND scintilla);

    // Wrap the selected lines in a fenced code block (or insert an empty
    // fence at the caret line); the caret lands on the info-string position.
    void InsertCodeFence(HWND scintilla);
};

}  // namespace nmf
