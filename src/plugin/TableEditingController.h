#pragma once

#include "core/TableEditing.h"

#include <optional>
#include <string>
#include <vector>
#include <windows.h>

namespace nmf {

// Scintilla glue for core/TableEditing: Tab-to-next-cell navigation and the
// table commands. All edits are single undo actions; callers gate to
// Markdown documents.
class TableEditingController {
public:
    void SetSmartTabEnabled(bool enabled);
    bool SmartTabEnabled() const;

    // Tab / Shift+Tab cell navigation, invoked from the Scintilla window
    // subclass before Scintilla sees the key. Returns true when the caret was
    // inside a pipe table and the key was consumed.
    bool NavigateCell(HWND scintilla, bool forward);

    void FormatTableAtCaret(HWND scintilla);
    void InsertColumn(HWND scintilla);
    void DeleteColumn(HWND scintilla);
    void CycleColumnAlignment(HWND scintilla);

private:
    struct TableContext {
        int firstLine{0};
        int lastLine{0};
        bool hasDelimiter{false};
        std::vector<std::string> lines;
        MarkdownTable table;
        int caretLine{0};
        size_t cellIndex{0};
        int bodyRowIndex{-1};  // -1 = header (or delimiter) line
    };

    std::optional<TableContext> LoadContext(HWND scintilla);
    void ReplaceBlock(HWND scintilla, const TableContext& context, const std::vector<std::string>& formatted);
    void SelectCell(HWND scintilla, int firstLine, const std::vector<std::string>& formatted, int bodyRowIndex, size_t cellIndex);

    bool smartTabEnabled_{true};
};

}  // namespace nmf
