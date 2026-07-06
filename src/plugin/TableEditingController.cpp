#include "plugin/TableEditingController.h"

#include "plugin/NppApi.h"

#include <algorithm>

namespace nmf {
namespace {

class UndoGroup {
public:
    explicit UndoGroup(HWND scintilla) : scintilla_(scintilla) {
        ::SendMessage(scintilla_, npp::SCI_BEGINUNDOACTION, 0, 0);
    }
    ~UndoGroup() {
        ::SendMessage(scintilla_, npp::SCI_ENDUNDOACTION, 0, 0);
    }

private:
    HWND scintilla_;
};

ptrdiff_t LineStart(HWND scintilla, int line) {
    return static_cast<ptrdiff_t>(::SendMessage(scintilla, npp::SCI_POSITIONFROMLINE, line, 0));
}

ptrdiff_t LineEnd(HWND scintilla, int line) {
    return static_cast<ptrdiff_t>(::SendMessage(scintilla, npp::SCI_GETLINEENDPOSITION, line, 0));
}

int LineCount(HWND scintilla) {
    return static_cast<int>(::SendMessage(scintilla, npp::SCI_GETLINECOUNT, 0, 0));
}

std::string EolText(HWND scintilla) {
    switch (::SendMessage(scintilla, npp::SCI_GETEOLMODE, 0, 0)) {
        case 0:
            return "\r\n";
        case 1:
            return "\r";
        default:
            return "\n";
    }
}

}  // namespace

void TableEditingController::SetSmartTabEnabled(bool enabled) {
    smartTabEnabled_ = enabled;
}

bool TableEditingController::SmartTabEnabled() const {
    return smartTabEnabled_;
}

std::optional<TableEditingController::TableContext> TableEditingController::LoadContext(HWND scintilla) {
    if (scintilla == nullptr) {
        return std::nullopt;
    }
    const auto position = static_cast<ptrdiff_t>(::SendMessage(scintilla, npp::SCI_GETCURRENTPOS, 0, 0));
    const int caretLine = static_cast<int>(::SendMessage(scintilla, npp::SCI_LINEFROMPOSITION, position, 0));
    const auto caretText = npp::ScintillaLineText(scintilla, caretLine);
    if (!IsTableLine(caretText)) {
        return std::nullopt;
    }

    TableContext context;
    context.caretLine = caretLine;
    context.firstLine = caretLine;
    while (context.firstLine > 0 && IsTableLine(npp::ScintillaLineText(scintilla, context.firstLine - 1))) {
        --context.firstLine;
    }
    context.lastLine = caretLine;
    const int lineCount = LineCount(scintilla);
    while (context.lastLine + 1 < lineCount && IsTableLine(npp::ScintillaLineText(scintilla, context.lastLine + 1))) {
        ++context.lastLine;
    }
    for (int line = context.firstLine; line <= context.lastLine; ++line) {
        context.lines.push_back(npp::ScintillaLineText(scintilla, line));
    }

    if (const auto parsed = ParseTable(context.lines)) {
        context.hasDelimiter = true;
        context.table = *parsed;
    } else {
        // Scaffold: treat the first line as the header, the rest as rows.
        context.hasDelimiter = false;
        context.table.indent = context.lines[0].substr(0, context.lines[0].find_first_not_of(" \t") == std::string::npos ? 0 : context.lines[0].find_first_not_of(" \t"));
        context.table.header = SplitTableRow(context.lines[0]);
        context.table.alignments.assign(context.table.header.size(), TableAlignment::None);
        for (size_t index = 1; index < context.lines.size(); ++index) {
            auto cells = SplitTableRow(context.lines[index]);
            cells.resize(context.table.header.size());
            context.table.rows.push_back(std::move(cells));
        }
    }

    const auto lineStart = LineStart(scintilla, caretLine);
    const auto byteColumn = static_cast<size_t>(std::max<ptrdiff_t>(0, position - lineStart));
    context.cellIndex = std::min(CellIndexForColumn(caretText, byteColumn), context.table.header.empty() ? size_t{0} : context.table.header.size() - 1);

    const int offsetInBlock = caretLine - context.firstLine;
    if (offsetInBlock == 0 || (context.hasDelimiter && offsetInBlock == 1)) {
        context.bodyRowIndex = -1;  // header or delimiter
    } else {
        context.bodyRowIndex = offsetInBlock - (context.hasDelimiter ? 2 : 1);
    }
    return context;
}

void TableEditingController::ReplaceBlock(HWND scintilla, const TableContext& context, const std::vector<std::string>& formatted) {
    const auto eol = EolText(scintilla);
    std::string replacement;
    for (size_t index = 0; index < formatted.size(); ++index) {
        if (index > 0) {
            replacement += eol;
        }
        replacement += formatted[index];
    }
    std::string existing;
    for (size_t index = 0; index < context.lines.size(); ++index) {
        if (index > 0) {
            existing += eol;
        }
        existing += context.lines[index];
    }
    if (replacement == existing) {
        return;
    }
    npp::ScintillaReplaceRange(scintilla, LineStart(scintilla, context.firstLine), LineEnd(scintilla, context.lastLine), replacement);
}

void TableEditingController::SelectCell(HWND scintilla, int firstLine, const std::vector<std::string>& formatted, int bodyRowIndex, size_t cellIndex) {
    const int lineOffset = bodyRowIndex < 0 ? 0 : 2 + bodyRowIndex;
    if (lineOffset >= static_cast<int>(formatted.size())) {
        return;
    }
    const int targetLine = firstLine + lineOffset;
    const auto ranges = FormattedCellRanges(formatted[static_cast<size_t>(lineOffset)]);
    if (ranges.empty()) {
        return;
    }
    // Place the caret at the cell's content start. Deliberately no selection:
    // pressing Tab while Scintilla has a selection indents/replaces instead of
    // firing SCN_CHARADDED, which would eat cell content during navigation.
    const auto& range = ranges[std::min(cellIndex, ranges.size() - 1)];
    const auto lineStart = LineStart(scintilla, targetLine);
    ::SendMessage(scintilla, npp::SCI_GOTOPOS, static_cast<WPARAM>(lineStart + static_cast<ptrdiff_t>(range.start)), 0);
}

bool TableEditingController::NavigateCell(HWND scintilla, bool forward) {
    if (!smartTabEnabled_ || scintilla == nullptr) {
        return false;
    }
    if (::SendMessage(scintilla, npp::SCI_GETSELECTIONS, 0, 0) != 1) {
        return false;
    }
    auto context = LoadContext(scintilla);
    if (!context) {
        return false;
    }

    UndoGroup undo(scintilla);
    const size_t columns = std::max<size_t>(context->table.header.size(), 1);
    int targetRow = context->bodyRowIndex;
    size_t targetCell = context->cellIndex;
    if (forward) {
        ++targetCell;
        if (targetCell >= columns) {
            targetCell = 0;
            ++targetRow;  // header (-1) advances to first body row (0)
        }
        if (targetRow >= static_cast<int>(context->table.rows.size())) {
            context->table.rows.emplace_back(columns, std::string{});
            targetRow = static_cast<int>(context->table.rows.size()) - 1;
        }
    } else {
        if (targetCell > 0) {
            --targetCell;
        } else if (targetRow >= 0) {
            --targetRow;  // first body row (0) retreats to the header (-1)
            targetCell = columns - 1;
        }
        // At the very first header cell: stay put (but still reformat).
    }

    const auto formatted = FormatTable(context->table);
    ReplaceBlock(scintilla, *context, formatted);
    SelectCell(scintilla, context->firstLine, formatted, targetRow, targetCell);
    return true;
}

void TableEditingController::FormatTableAtCaret(HWND scintilla) {
    auto context = LoadContext(scintilla);
    if (!context) {
        return;
    }
    UndoGroup undo(scintilla);
    if (!context->hasDelimiter && context->table.rows.empty()) {
        // Scaffolding a fresh header: give the user a row to type into.
        context->table.rows.emplace_back(context->table.header.size(), std::string{});
    }
    const auto formatted = FormatTable(context->table);
    ReplaceBlock(scintilla, *context, formatted);
    SelectCell(scintilla, context->firstLine, formatted, context->bodyRowIndex, context->cellIndex);
}

void TableEditingController::InsertColumn(HWND scintilla) {
    auto context = LoadContext(scintilla);
    if (!context) {
        return;
    }
    UndoGroup undo(scintilla);
    InsertTableColumn(context->table, context->cellIndex);
    const auto formatted = FormatTable(context->table);
    ReplaceBlock(scintilla, *context, formatted);
    SelectCell(scintilla, context->firstLine, formatted, context->bodyRowIndex, context->cellIndex + 1);
}

void TableEditingController::DeleteColumn(HWND scintilla) {
    auto context = LoadContext(scintilla);
    if (!context) {
        return;
    }
    UndoGroup undo(scintilla);
    if (!DeleteTableColumn(context->table, context->cellIndex)) {
        return;
    }
    const auto formatted = FormatTable(context->table);
    ReplaceBlock(scintilla, *context, formatted);
    const size_t columns = context->table.header.size();
    SelectCell(scintilla, context->firstLine, formatted, context->bodyRowIndex, std::min(context->cellIndex, columns - 1));
}

void TableEditingController::CycleColumnAlignment(HWND scintilla) {
    auto context = LoadContext(scintilla);
    if (!context) {
        return;
    }
    UndoGroup undo(scintilla);
    CycleTableColumnAlignment(context->table, context->cellIndex);
    const auto formatted = FormatTable(context->table);
    ReplaceBlock(scintilla, *context, formatted);
    SelectCell(scintilla, context->firstLine, formatted, context->bodyRowIndex, context->cellIndex);
}

}  // namespace nmf
