#include "plugin/NppApi.h"

#include <string>
#include <vector>
#include <algorithm>

namespace nmf::npp {

HWND CurrentScintilla(const NppData& data) {
    int current = 0;
    ::SendMessage(data._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&current));
    if (current == 1) {
        return data._scintillaSecondHandle;
    }
    return data._scintillaMainHandle;
}

std::wstring CurrentFilePath(HWND nppHandle) {
    std::wstring buffer(32768, L'\0');
    const auto ok = ::SendMessage(nppHandle, NPPM_GETFULLCURRENTPATH, buffer.size(), reinterpret_cast<LPARAM>(buffer.data()));
    if (!ok) {
        return {};
    }
    buffer.resize(wcsnlen_s(buffer.c_str(), buffer.size()));
    return buffer;
}

std::wstring PluginConfigDirectory(HWND nppHandle) {
    const auto required = static_cast<size_t>(::SendMessage(nppHandle, NPPM_GETPLUGINSCONFIGDIR, 0, 0));
    if (required == 0) {
        return {};
    }
    std::wstring buffer(required + 1, L'\0');
    const auto ok = ::SendMessage(nppHandle, NPPM_GETPLUGINSCONFIGDIR, buffer.size(), reinterpret_cast<LPARAM>(buffer.data()));
    if (!ok) {
        return {};
    }
    buffer.resize(wcsnlen_s(buffer.c_str(), buffer.size()));
    return buffer;
}

std::string CurrentScintillaText(HWND scintilla) {
    if (scintilla == nullptr) {
        return {};
    }
    const auto length = static_cast<size_t>(::SendMessage(scintilla, SCI_GETLENGTH, 0, 0));
    std::string buffer(length + 1, '\0');
    ::SendMessage(scintilla, SCI_GETTEXT, buffer.size(), reinterpret_cast<LPARAM>(buffer.data()));
    buffer.resize(length);
    return buffer;
}

bool CurrentScintillaIsUtf8(HWND scintilla) {
    if (scintilla == nullptr) {
        return true;
    }
    return ::SendMessage(scintilla, SCI_GETCODEPAGE, 0, 0) == SC_CP_UTF8;
}

void SetStatus(HWND nppHandle, const std::wstring& text) {
    if (nppHandle != nullptr) {
        ::SendMessage(nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_TYPING_MODE, reinterpret_cast<LPARAM>(text.c_str()));
    }
}

void SetMenuChecked(HWND nppHandle, int commandId, bool checked) {
    if (nppHandle != nullptr && commandId != 0) {
        ::SendMessage(nppHandle, NPPM_SETMENUITEMCHECK, commandId, checked ? TRUE : FALSE);
    }
}

double ScintillaViewportRatio(HWND scintilla) {
    if (scintilla == nullptr) {
        return 0.0;
    }
    const auto firstVisible = static_cast<double>(::SendMessage(scintilla, SCI_GETFIRSTVISIBLELINE, 0, 0));
    const auto lineCount = static_cast<double>(::SendMessage(scintilla, SCI_GETLINECOUNT, 0, 0));
    const auto linesOnScreen = static_cast<double>(::SendMessage(scintilla, SCI_LINESONSCREEN, 0, 0));
    const auto maxFirstVisible = std::max(1.0, lineCount - linesOnScreen);
    return std::clamp(firstVisible / maxFirstVisible, 0.0, 1.0);
}

void SetScintillaViewportRatio(HWND scintilla, double ratio) {
    if (scintilla == nullptr) {
        return;
    }
    ratio = std::clamp(ratio, 0.0, 1.0);
    const auto current = static_cast<int>(::SendMessage(scintilla, SCI_GETFIRSTVISIBLELINE, 0, 0));
    const auto lineCount = static_cast<double>(::SendMessage(scintilla, SCI_GETLINECOUNT, 0, 0));
    const auto linesOnScreen = static_cast<double>(::SendMessage(scintilla, SCI_LINESONSCREEN, 0, 0));
    const auto maxFirstVisible = std::max(0.0, lineCount - linesOnScreen);
    const auto target = static_cast<int>(maxFirstVisible * ratio);
    ::SendMessage(scintilla, SCI_LINESCROLL, 0, target - current);
}

}  // namespace nmf::npp
