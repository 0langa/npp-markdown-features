#pragma once

#include <windows.h>

namespace nmf {

// Scintilla-side glue for core/ListEditing: smart Enter continuation and the
// checkbox/renumber commands. All document edits are grouped as single undo
// actions. Callers gate invocations to Markdown documents.
class ListEditingController {
public:
    void SetSmartEnterEnabled(bool enabled);
    bool SmartEnterEnabled() const;

    // Called from SCN_CHARADDED; acts only on Enter with a single caret.
    void OnCharAdded(HWND scintilla, int ch);

    // Toggle/add task checkboxes on all lines covered by the selection.
    void ToggleCheckbox(HWND scintilla);

    // Renumber the contiguous list block around the caret.
    void RenumberAtCaret(HWND scintilla);

private:
    void RenumberBlockAroundLine(HWND scintilla, int line);

    bool smartEnterEnabled_{true};
};

}  // namespace nmf
