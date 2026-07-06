#include "plugin/ListEditingController.h"

#include "core/ListEditing.h"
#include "plugin/NppApi.h"

#include <string>
#include <vector>

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

size_t LeadingWhitespaceLength(const std::string& text) {
    size_t index = 0;
    while (index < text.size() && (text[index] == ' ' || text[index] == '\t')) {
        ++index;
    }
    return index;
}

}  // namespace

void ListEditingController::SetSmartEnterEnabled(bool enabled) {
    smartEnterEnabled_ = enabled;
}

bool ListEditingController::SmartEnterEnabled() const {
    return smartEnterEnabled_;
}

void ListEditingController::OnCharAdded(HWND scintilla, int ch) {
    if (!smartEnterEnabled_ || scintilla == nullptr) {
        return;
    }
    if (ch != '\n' && ch != '\r') {
        return;
    }
    // In CRLF mode Scintilla reports the '\r' first and the '\n' second;
    // act once, on the '\n'.
    if (ch == '\r' && ::SendMessage(scintilla, npp::SCI_GETEOLMODE, 0, 0) == npp::SC_EOL_CRLF) {
        return;
    }
    if (::SendMessage(scintilla, npp::SCI_GETSELECTIONS, 0, 0) != 1) {
        return;
    }

    const auto position = static_cast<ptrdiff_t>(::SendMessage(scintilla, npp::SCI_GETCURRENTPOS, 0, 0));
    const int line = static_cast<int>(::SendMessage(scintilla, npp::SCI_LINEFROMPOSITION, position, 0));
    if (line <= 0) {
        return;
    }
    const auto previousLine = npp::ScintillaLineText(scintilla, line - 1);
    const auto continuation = ContinuationForLine(previousLine);
    if (!continuation) {
        return;
    }

    UndoGroup undo(scintilla);
    if (continuation->terminate) {
        // Empty item: remove the marker line (including its EOL); the caret
        // ends up on the now-empty line where the item used to be.
        const auto previousStart = LineStart(scintilla, line - 1);
        const auto currentStart = LineStart(scintilla, line);
        npp::ScintillaReplaceRange(scintilla, previousStart, currentStart, "");
        ::SendMessage(scintilla, npp::SCI_GOTOPOS, static_cast<WPARAM>(previousStart), 0);
        return;
    }

    // Replace any auto-indent whitespace at the caret line start with the
    // continuation prefix, keeping text that was split off the previous line.
    const auto lineStart = LineStart(scintilla, line);
    const auto lineText = npp::ScintillaLineText(scintilla, line);
    const auto whitespace = static_cast<ptrdiff_t>(LeadingWhitespaceLength(lineText));
    npp::ScintillaReplaceRange(scintilla, lineStart, lineStart + whitespace, continuation->prefix);
    ::SendMessage(scintilla, npp::SCI_GOTOPOS, static_cast<WPARAM>(lineStart + static_cast<ptrdiff_t>(continuation->prefix.size())), 0);

    // Keep the numbers below an ordered continuation consistent.
    const auto inserted = ParseListItem(continuation->prefix);
    if (inserted && inserted->isOrdered) {
        RenumberBlockAroundLine(scintilla, line);
    }
}

void ListEditingController::ToggleCheckbox(HWND scintilla) {
    if (scintilla == nullptr) {
        return;
    }
    const auto selectionStart = static_cast<ptrdiff_t>(::SendMessage(scintilla, npp::SCI_GETSELECTIONSTART, 0, 0));
    const auto selectionEnd = static_cast<ptrdiff_t>(::SendMessage(scintilla, npp::SCI_GETSELECTIONEND, 0, 0));
    const int firstLine = static_cast<int>(::SendMessage(scintilla, npp::SCI_LINEFROMPOSITION, selectionStart, 0));
    const int lastLine = static_cast<int>(::SendMessage(scintilla, npp::SCI_LINEFROMPOSITION, selectionEnd, 0));
    const bool multiLine = lastLine > firstLine;

    UndoGroup undo(scintilla);
    for (int line = firstLine; line <= lastLine; ++line) {
        const auto text = npp::ScintillaLineText(scintilla, line);
        if (multiLine && text.find_first_not_of(" \t") == std::string::npos) {
            continue;  // don't grow blank lines inside a selection
        }
        const auto toggled = ToggleTaskCheckbox(text);
        if (toggled != text) {
            npp::ScintillaReplaceRange(scintilla, LineStart(scintilla, line), LineEnd(scintilla, line), toggled);
        }
    }
    if (!multiLine) {
        ::SendMessage(scintilla, npp::SCI_GOTOPOS, static_cast<WPARAM>(LineEnd(scintilla, firstLine)), 0);
    }
}

void ListEditingController::RenumberAtCaret(HWND scintilla) {
    if (scintilla == nullptr) {
        return;
    }
    const auto position = static_cast<ptrdiff_t>(::SendMessage(scintilla, npp::SCI_GETCURRENTPOS, 0, 0));
    const int line = static_cast<int>(::SendMessage(scintilla, npp::SCI_LINEFROMPOSITION, position, 0));
    UndoGroup undo(scintilla);
    RenumberBlockAroundLine(scintilla, line);
}

void ListEditingController::RenumberBlockAroundLine(HWND scintilla, int line) {
    const int lineCount = LineCount(scintilla);
    if (line < 0 || line >= lineCount) {
        return;
    }
    if (!IsListBlockLine(npp::ScintillaLineText(scintilla, line))) {
        return;
    }
    int first = line;
    while (first > 0 && IsListBlockLine(npp::ScintillaLineText(scintilla, first - 1))) {
        --first;
    }
    int last = line;
    while (last + 1 < lineCount && IsListBlockLine(npp::ScintillaLineText(scintilla, last + 1))) {
        ++last;
    }

    std::vector<std::string> block;
    block.reserve(static_cast<size_t>(last - first + 1));
    for (int index = first; index <= last; ++index) {
        block.push_back(npp::ScintillaLineText(scintilla, index));
    }
    const auto renumbered = RenumberListBlock(block);
    for (int index = first; index <= last; ++index) {
        const auto& updated = renumbered[static_cast<size_t>(index - first)];
        if (updated != block[static_cast<size_t>(index - first)]) {
            npp::ScintillaReplaceRange(scintilla, LineStart(scintilla, index), LineEnd(scintilla, index), updated);
        }
    }
}

}  // namespace nmf
