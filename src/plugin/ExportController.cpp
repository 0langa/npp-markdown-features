#include "plugin/ExportController.h"

#include "core/HtmlExport.h"
#include "core/Strings.h"
#include "plugin/NppApi.h"

#include <commdlg.h>
#include <filesystem>
#include <fstream>

namespace nmf {
namespace {

std::string SelectionOrAll(HWND scintilla, const std::string& fullText) {
    const auto start = static_cast<ptrdiff_t>(::SendMessage(scintilla, npp::SCI_GETSELECTIONSTART, 0, 0));
    const auto end = static_cast<ptrdiff_t>(::SendMessage(scintilla, npp::SCI_GETSELECTIONEND, 0, 0));
    if (start >= end || start < 0 || static_cast<size_t>(end) > fullText.size()) {
        return fullText;
    }
    return fullText.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
}

bool SetClipboardPayloads(HWND owner, const std::string& cfHtmlPayload, const std::wstring& plainText) {
    if (!::OpenClipboard(owner)) {
        return false;
    }
    ::EmptyClipboard();
    bool ok = true;

    const UINT htmlFormat = ::RegisterClipboardFormat(L"HTML Format");
    if (htmlFormat != 0) {
        const size_t bytes = cfHtmlPayload.size() + 1;
        if (HGLOBAL handle = ::GlobalAlloc(GMEM_MOVEABLE, bytes)) {
            if (void* buffer = ::GlobalLock(handle)) {
                memcpy(buffer, cfHtmlPayload.c_str(), bytes);
                ::GlobalUnlock(handle);
                if (::SetClipboardData(htmlFormat, handle) == nullptr) {
                    ::GlobalFree(handle);
                    ok = false;
                }
            } else {
                ::GlobalFree(handle);
                ok = false;
            }
        }
    }

    const size_t wideBytes = (plainText.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL handle = ::GlobalAlloc(GMEM_MOVEABLE, wideBytes)) {
        if (void* buffer = ::GlobalLock(handle)) {
            memcpy(buffer, plainText.c_str(), wideBytes);
            ::GlobalUnlock(handle);
            if (::SetClipboardData(CF_UNICODETEXT, handle) == nullptr) {
                ::GlobalFree(handle);
                ok = false;
            }
        } else {
            ::GlobalFree(handle);
            ok = false;
        }
    }

    ::CloseClipboard();
    return ok;
}

}  // namespace

std::wstring ExportController::ExportHtml(HWND scintilla, HWND npp, const std::wstring& documentPath, const std::string& markdownUtf8) {
    if (scintilla == nullptr) {
        return L"Markdown Features: no editor";
    }
    std::filesystem::path defaultPath;
    if (!documentPath.empty()) {
        defaultPath = std::filesystem::path(documentPath).replace_extension(L".html");
    } else {
        defaultPath = L"export.html";
    }

    wchar_t fileBuffer[MAX_PATH]{};
    wcsncpy_s(fileBuffer, defaultPath.filename().wstring().c_str(), _TRUNCATE);
    const auto initialDir = defaultPath.has_parent_path() ? defaultPath.parent_path().wstring() : std::wstring{};

    OPENFILENAME dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = npp;
    dialog.lpstrFilter = L"HTML files (*.html)\0*.html\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = fileBuffer;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = L"html";
    dialog.lpstrInitialDir = initialDir.empty() ? nullptr : initialDir.c_str();
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN;
    if (!::GetSaveFileName(&dialog)) {
        return L"Markdown Features: export cancelled";
    }

    try {
        const auto rendered = renderer_.Render(markdownUtf8, documentPath);
        const auto html = RemoveSourcepos(rendered.documentHtml);
        std::ofstream output(fileBuffer, std::ios::binary | std::ios::trunc);
        if (!output) {
            return L"Markdown Features: could not write file";
        }
        output << html;
        return std::wstring(L"Markdown Features: exported ") + fileBuffer;
    } catch (...) {
        return L"Markdown Features: export failed";
    }
}

std::wstring ExportController::CopyAsHtml(HWND scintilla, const std::wstring& documentPath, const std::string& markdownUtf8) {
    if (scintilla == nullptr) {
        return L"Markdown Features: no editor";
    }
    try {
        const auto source = SelectionOrAll(scintilla, markdownUtf8);
        const auto rendered = renderer_.Render(source, documentPath);
        const auto fragment = RemoveSourcepos(rendered.bodyHtml);
        const auto payload = BuildClipboardHtml(fragment);
        if (!SetClipboardPayloads(scintilla, payload, Utf8ToWide(fragment))) {
            return L"Markdown Features: clipboard unavailable";
        }
        return L"Markdown Features: HTML copied to clipboard";
    } catch (...) {
        return L"Markdown Features: copy failed";
    }
}

}  // namespace nmf
