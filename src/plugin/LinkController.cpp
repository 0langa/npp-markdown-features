#include "plugin/LinkController.h"

#include "core/LinkTools.h"
#include "core/MarkdownOutline.h"
#include "core/Strings.h"
#include "plugin/NppApi.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <shellapi.h>
#include <sstream>
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

std::vector<std::string> DocumentLines(HWND scintilla) {
    std::vector<std::string> lines;
    const int count = static_cast<int>(::SendMessage(scintilla, npp::SCI_GETLINECOUNT, 0, 0));
    lines.reserve(static_cast<size_t>(count));
    for (int line = 0; line < count; ++line) {
        lines.push_back(npp::ScintillaLineText(scintilla, line));
    }
    return lines;
}

std::wstring ReadClipboardText() {
    if (!::OpenClipboard(nullptr)) {
        return {};
    }
    std::wstring text;
    if (HANDLE handle = ::GetClipboardData(CF_UNICODETEXT)) {
        if (const auto* data = static_cast<const wchar_t*>(::GlobalLock(handle))) {
            text = data;
            ::GlobalUnlock(handle);
        }
    }
    ::CloseClipboard();
    return text;
}

std::wstring TrimWide(const std::wstring& value) {
    const auto begin = value.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(L" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

bool LooksLikeUrl(const std::string& value) {
    return ClassifyLinkTarget(value) == LinkKind::External;
}

// Find the heading line matching a "#fragment" (GitHub slug or raw text).
int FindAnchorLine(const std::vector<std::string>& lines, const std::string& fragment) {
    std::string joined;
    for (const auto& line : lines) {
        joined += line;
        joined += '\n';
    }
    const auto outline = MarkdownOutline::Parse(joined);
    std::string wanted = fragment;
    std::transform(wanted.begin(), wanted.end(), wanted.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    for (const auto& heading : outline.Headings()) {
        if (GithubSlug(heading.text) == wanted || heading.anchorId == "nmf-heading-" + wanted) {
            return heading.line;
        }
    }
    return -1;
}

std::filesystem::path ResolveLocalPath(const std::wstring& documentPath, const std::string& utf8Relative) {
    const std::filesystem::path relative(Utf8ToWide(utf8Relative));
    if (relative.is_absolute()) {
        return relative;
    }
    const std::filesystem::path document(documentPath);
    return document.parent_path() / relative;
}

}  // namespace

void LinkController::SetPasteUrlEnabled(bool enabled) {
    pasteUrlEnabled_ = enabled;
}

bool LinkController::PasteUrlEnabled() const {
    return pasteUrlEnabled_;
}

void LinkController::InsertLink(HWND scintilla, bool image) {
    if (scintilla == nullptr || ::SendMessage(scintilla, npp::SCI_GETSELECTIONS, 0, 0) != 1) {
        return;
    }
    const auto start = Position(scintilla, npp::SCI_GETSELECTIONSTART);
    const auto end = Position(scintilla, npp::SCI_GETSELECTIONEND);
    const auto selected = TextAt(scintilla, start, end);

    UndoGroup undo(scintilla);
    const std::string prefix = image ? "![" : "[";
    const std::string replacement = prefix + selected + "]()";
    npp::ScintillaReplaceRange(scintilla, start, end, replacement);
    if (selected.empty()) {
        // Caret between the brackets to type the text first.
        ::SendMessage(scintilla, npp::SCI_GOTOPOS, static_cast<WPARAM>(start + static_cast<ptrdiff_t>(prefix.size())), 0);
    } else {
        // Caret between the parens to type the URL.
        ::SendMessage(scintilla, npp::SCI_GOTOPOS, static_cast<WPARAM>(start + static_cast<ptrdiff_t>(replacement.size()) - 1), 0);
    }
}

std::wstring LinkController::FollowLink(HWND scintilla, HWND npp, const std::wstring& documentPath) {
    if (scintilla == nullptr) {
        return L"Markdown Features: no editor";
    }
    const auto caret = Position(scintilla, npp::SCI_GETCURRENTPOS);
    const int line = static_cast<int>(Position(scintilla, npp::SCI_LINEFROMPOSITION, static_cast<WPARAM>(caret)));
    const auto lineStart = Position(scintilla, npp::SCI_POSITIONFROMLINE, static_cast<WPARAM>(line));
    const auto lineText = npp::ScintillaLineText(scintilla, line);
    auto link = LinkAtPosition(lineText, static_cast<size_t>(caret - lineStart));
    if (!link) {
        return L"Markdown Features: no link at caret";
    }

    std::string target = link->target;
    if (!link->reference.empty()) {
        const auto resolved = ResolveReference(DocumentLines(scintilla), link->reference);
        if (!resolved) {
            return L"Markdown Features: unresolved reference [" + Utf8ToWide(link->reference) + L"]";
        }
        target = *resolved;
    }
    if (target.empty()) {
        return L"Markdown Features: link has no destination";
    }

    switch (ClassifyLinkTarget(target)) {
        case LinkKind::External: {
            ::ShellExecute(nullptr, L"open", Utf8ToWide(target).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return L"Markdown Features: opened " + Utf8ToWide(target);
        }
        case LinkKind::Anchor: {
            const auto anchorLine = FindAnchorLine(DocumentLines(scintilla), target.substr(1));
            if (anchorLine < 0) {
                return L"Markdown Features: heading not found for " + Utf8ToWide(target);
            }
            ::SendMessage(scintilla, npp::SCI_ENSUREVISIBLE, anchorLine, 0);
            ::SendMessage(scintilla, npp::SCI_GOTOLINE, anchorLine, 0);
            npp::SetScintillaFirstVisibleLine(scintilla, anchorLine);
            return L"Markdown Features: jumped to heading";
        }
        case LinkKind::LocalFile:
        default: {
            const auto local = SplitLocalTarget(target);
            if (local.path.empty()) {
                return L"Markdown Features: empty link target";
            }
            const auto resolved = ResolveLocalPath(documentPath, local.path);
            std::error_code ec;
            if (!std::filesystem::exists(resolved, ec)) {
                return L"Markdown Features: not found: " + resolved.wstring();
            }
            if (std::filesystem::is_directory(resolved, ec)) {
                ::ShellExecute(nullptr, L"open", resolved.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                return L"Markdown Features: opened folder";
            }
            ::SendMessage(npp, npp::NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(resolved.wstring().c_str()));
            return L"Markdown Features: opened " + resolved.filename().wstring();
        }
    }
}

LinkController::CheckResult LinkController::CheckLinks(HWND scintilla, const std::wstring& documentPath) {
    if (scintilla == nullptr) {
        return {L"Markdown Features: no editor", {}};
    }
    const auto lines = DocumentLines(scintilla);
    int totalLinks = 0;
    int externalLinks = 0;
    std::ostringstream issues;
    int issueCount = 0;

    const auto addIssue = [&](int line, const std::string& target, const std::string& reason) {
        issues << "line " << (line + 1) << ": " << (target.empty() ? "(empty)" : target) << " - " << reason << "\n";
        ++issueCount;
    };

    for (int lineIndex = 0; lineIndex < static_cast<int>(lines.size()); ++lineIndex) {
        for (const auto& link : LinksOnLine(lines[static_cast<size_t>(lineIndex)])) {
            ++totalLinks;
            std::string target = link.target;
            if (!link.reference.empty()) {
                const auto resolved = ResolveReference(lines, link.reference);
                if (!resolved) {
                    addIssue(lineIndex, "[" + link.reference + "]", "unresolved reference");
                    continue;
                }
                target = *resolved;
            }
            if (target.empty()) {
                addIssue(lineIndex, link.text, "empty link destination");
                continue;
            }
            switch (ClassifyLinkTarget(target)) {
                case LinkKind::External:
                    ++externalLinks;  // not validated (no network access)
                    break;
                case LinkKind::Anchor: {
                    if (FindAnchorLine(lines, target.substr(1)) < 0) {
                        addIssue(lineIndex, target, "no matching heading");
                    }
                    break;
                }
                case LinkKind::LocalFile:
                default: {
                    const auto local = SplitLocalTarget(target);
                    if (local.path.empty()) {
                        break;
                    }
                    const auto resolved = ResolveLocalPath(documentPath, local.path);
                    std::error_code ec;
                    if (!std::filesystem::exists(resolved, ec)) {
                        addIssue(lineIndex, target, "file not found");
                    }
                    break;
                }
            }
        }
    }

    if (issueCount == 0) {
        std::wostringstream status;
        status << L"Markdown Features: links OK (" << totalLinks << L" found, " << externalLinks << L" external not checked)";
        return {status.str(), {}};
    }

    std::ostringstream report;
    report << "Markdown link check: " << issueCount << " issue(s)\n";
    report << "Document: " << WideToUtf8(documentPath) << "\n\n";
    report << issues.str();
    std::wostringstream status;
    status << L"Markdown Features: " << issueCount << L" link issue(s) found";
    return {status.str(), report.str()};
}

bool LinkController::HandlePaste(HWND scintilla) {
    if (!pasteUrlEnabled_ || scintilla == nullptr) {
        return false;
    }
    if (::SendMessage(scintilla, npp::SCI_GETSELECTIONS, 0, 0) != 1) {
        return false;
    }
    const auto start = Position(scintilla, npp::SCI_GETSELECTIONSTART);
    const auto end = Position(scintilla, npp::SCI_GETSELECTIONEND);
    if (start >= end) {
        return false;
    }
    const auto clipboard = WideToUtf8(TrimWide(ReadClipboardText()));
    if (clipboard.empty() || clipboard.find('\n') != std::string::npos || !LooksLikeUrl(clipboard)) {
        return false;
    }
    const auto selected = TextAt(scintilla, start, end);
    if (LooksLikeUrl(selected)) {
        return false;  // pasting a URL over a URL: let the normal paste happen
    }

    UndoGroup undo(scintilla);
    const std::string replacement = "[" + selected + "](" + clipboard + ")";
    npp::ScintillaReplaceRange(scintilla, start, end, replacement);
    ::SendMessage(scintilla, npp::SCI_GOTOPOS, static_cast<WPARAM>(start + static_cast<ptrdiff_t>(replacement.size())), 0);
    return true;
}

}  // namespace nmf
