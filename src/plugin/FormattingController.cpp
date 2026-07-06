#include "plugin/FormattingController.h"

#include "core/InlineFormatting.h"
#include "plugin/NppApi.h"

#include <algorithm>
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

ptrdiff_t Position(HWND scintilla, UINT message, WPARAM wParam = 0, LPARAM lParam = 0) {
    return static_cast<ptrdiff_t>(::SendMessage(scintilla, message, wParam, lParam));
}

ptrdiff_t LineStart(HWND scintilla, int line) {
    return Position(scintilla, npp::SCI_POSITIONFROMLINE, static_cast<WPARAM>(line));
}

ptrdiff_t LineEnd(HWND scintilla, int line) {
    return Position(scintilla, npp::SCI_GETLINEENDPOSITION, static_cast<WPARAM>(line));
}

std::string TextAt(HWND scintilla, ptrdiff_t start, ptrdiff_t end) {
    std::string text;
    for (ptrdiff_t index = start; index < end; ++index) {
        const auto ch = static_cast<char>(::SendMessage(scintilla, npp::SCI_GETCHARAT, static_cast<WPARAM>(index), 0));
        if (ch == '\0') {
            break;
        }
        text.push_back(ch);
    }
    return text;
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

struct LineSpan {
    int first{0};
    int last{0};
    std::vector<std::string> lines;
};

LineSpan SelectedLines(HWND scintilla) {
    const auto selectionStart = Position(scintilla, npp::SCI_GETSELECTIONSTART);
    auto selectionEnd = Position(scintilla, npp::SCI_GETSELECTIONEND);
    LineSpan span;
    span.first = static_cast<int>(Position(scintilla, npp::SCI_LINEFROMPOSITION, static_cast<WPARAM>(selectionStart)));
    // A selection ending exactly at a line start shouldn't include that line.
    if (selectionEnd > selectionStart && selectionEnd == Position(scintilla, npp::SCI_POSITIONFROMLINE, static_cast<WPARAM>(Position(scintilla, npp::SCI_LINEFROMPOSITION, static_cast<WPARAM>(selectionEnd))))) {
        --selectionEnd;
    }
    span.last = static_cast<int>(Position(scintilla, npp::SCI_LINEFROMPOSITION, static_cast<WPARAM>(std::max(selectionStart, selectionEnd))));
    for (int line = span.first; line <= span.last; ++line) {
        span.lines.push_back(npp::ScintillaLineText(scintilla, line));
    }
    return span;
}

void ReplaceLines(HWND scintilla, const LineSpan& span, const std::vector<std::string>& replacement) {
    const auto eol = EolText(scintilla);
    std::string joined;
    for (size_t index = 0; index < replacement.size(); ++index) {
        if (index > 0) {
            joined += eol;
        }
        joined += replacement[index];
    }
    npp::ScintillaReplaceRange(scintilla, LineStart(scintilla, span.first), LineEnd(scintilla, span.last), joined);
}

}  // namespace

void FormattingController::ToggleInlineMarker(HWND scintilla, const std::string& marker) {
    if (scintilla == nullptr || ::SendMessage(scintilla, npp::SCI_GETSELECTIONS, 0, 0) != 1) {
        return;
    }
    auto start = Position(scintilla, npp::SCI_GETSELECTIONSTART);
    auto end = Position(scintilla, npp::SCI_GETSELECTIONEND);
    if (start == end) {
        // No selection: use the word at the caret.
        const auto caret = Position(scintilla, npp::SCI_GETCURRENTPOS);
        start = Position(scintilla, npp::SCI_WORDSTARTPOSITION, static_cast<WPARAM>(caret), TRUE);
        end = Position(scintilla, npp::SCI_WORDENDPOSITION, static_cast<WPARAM>(caret), TRUE);
        if (start == end) {
            return;
        }
    }

    const auto markerLength = static_cast<ptrdiff_t>(marker.size());
    const auto contextLength = markerLength + 1;
    const auto before = TextAt(scintilla, std::max<ptrdiff_t>(0, start - contextLength), start);
    const auto after = TextAt(scintilla, end, end + contextLength);
    const auto selected = TextAt(scintilla, start, end);

    UndoGroup undo(scintilla);
    // Selection that already includes the markers ("**text**" selected).
    if (static_cast<size_t>(end - start) > 2 * marker.size()
        && selected.compare(0, marker.size(), marker) == 0
        && selected.compare(selected.size() - marker.size(), marker.size(), marker) == 0
        && !ShouldUnwrapMarker(before, after, marker)) {
        const auto inner = selected.substr(marker.size(), selected.size() - 2 * marker.size());
        npp::ScintillaReplaceRange(scintilla, start, end, inner);
        ::SendMessage(scintilla, npp::SCI_SETSEL, static_cast<WPARAM>(start), static_cast<LPARAM>(start + static_cast<ptrdiff_t>(inner.size())));
        return;
    }
    if (ShouldUnwrapMarker(before, after, marker)) {
        // Remove the surrounding markers (outer edit first to keep offsets valid).
        npp::ScintillaReplaceRange(scintilla, end, end + markerLength, "");
        npp::ScintillaReplaceRange(scintilla, start - markerLength, start, "");
        ::SendMessage(scintilla, npp::SCI_SETSEL, static_cast<WPARAM>(start - markerLength), static_cast<LPARAM>(end - markerLength));
        return;
    }
    npp::ScintillaReplaceRange(scintilla, end, end, marker);
    npp::ScintillaReplaceRange(scintilla, start, start, marker);
    ::SendMessage(scintilla, npp::SCI_SETSEL, static_cast<WPARAM>(start + markerLength), static_cast<LPARAM>(end + markerLength));
}

void FormattingController::ChangeHeading(HWND scintilla, int delta) {
    if (scintilla == nullptr) {
        return;
    }
    const auto span = SelectedLines(scintilla);
    std::vector<std::string> replacement;
    replacement.reserve(span.lines.size());
    for (const auto& line : span.lines) {
        replacement.push_back(ChangeHeadingLevel(line, delta));
    }
    if (replacement == span.lines) {
        return;
    }
    UndoGroup undo(scintilla);
    ReplaceLines(scintilla, span, replacement);
    ::SendMessage(scintilla, npp::SCI_GOTOPOS, static_cast<WPARAM>(LineEnd(scintilla, span.first)), 0);
}

void FormattingController::ToggleBlockquote(HWND scintilla) {
    if (scintilla == nullptr) {
        return;
    }
    const auto span = SelectedLines(scintilla);
    const auto replacement = ToggleBlockquoteLines(span.lines);
    if (replacement == span.lines) {
        return;
    }
    UndoGroup undo(scintilla);
    ReplaceLines(scintilla, span, replacement);
    ::SendMessage(scintilla, npp::SCI_GOTOPOS, static_cast<WPARAM>(LineEnd(scintilla, span.last)), 0);
}

void FormattingController::InsertCodeFence(HWND scintilla) {
    if (scintilla == nullptr) {
        return;
    }
    const auto eol = EolText(scintilla);
    const auto span = SelectedLines(scintilla);
    const auto selectionStart = Position(scintilla, npp::SCI_GETSELECTIONSTART);
    const auto selectionEnd = Position(scintilla, npp::SCI_GETSELECTIONEND);

    UndoGroup undo(scintilla);
    if (selectionStart == selectionEnd) {
        // Empty fence block at the caret line.
        const auto lineStart = LineStart(scintilla, span.first);
        const std::string block = "```" + eol + eol + "```" + eol;
        npp::ScintillaReplaceRange(scintilla, lineStart, lineStart, block);
        ::SendMessage(scintilla, npp::SCI_GOTOPOS, static_cast<WPARAM>(lineStart + 3), 0);
        return;
    }
    // Wrap the selected lines.
    const auto endPosition = LineEnd(scintilla, span.last);
    npp::ScintillaReplaceRange(scintilla, endPosition, endPosition, eol + "```");
    const auto startPosition = LineStart(scintilla, span.first);
    const std::string opening = "```" + eol;
    npp::ScintillaReplaceRange(scintilla, startPosition, startPosition, opening);
    ::SendMessage(scintilla, npp::SCI_GOTOPOS, static_cast<WPARAM>(startPosition + 3), 0);
}

}  // namespace nmf
