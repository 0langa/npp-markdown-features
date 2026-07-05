#pragma once

#include <string>
#include <windows.h>

namespace nmf::npp {

constexpr UINT NPPMSG = WM_USER + 1000;
constexpr UINT NPPM_GETCURRENTSCINTILLA = 2028;
constexpr UINT NPPM_GETCURRENTLANGTYPE = 2029;
constexpr UINT NPPM_GETFULLCURRENTPATH = 4025;
constexpr UINT NPPM_GETPLUGINSCONFIGDIR = 2070;
constexpr UINT NPPM_SETSTATUSBAR = 2048;
constexpr UINT NPPM_SETMENUITEMCHECK = 2064;
constexpr UINT NPPM_ADDTOOLBARICON_FORDARKMODE = 2125;

constexpr UINT NPPN_READY = 1001;
constexpr UINT NPPN_TBMODIFICATION = 1002;
constexpr UINT NPPN_FILEOPENED = 1004;
constexpr UINT NPPN_FILEBEFORECLOSE = 1003;
constexpr UINT NPPN_FILECLOSED = 1005;
constexpr UINT NPPN_FILESAVED = 1008;
constexpr UINT NPPN_SHUTDOWN = 1009;
constexpr UINT NPPN_BUFFERACTIVATED = 1010;
constexpr UINT NPPN_LANGCHANGED = 1011;
constexpr UINT NPPN_DARKMODECHANGED = 1027;

constexpr UINT SCI_GETLENGTH = 2006;
constexpr UINT SCI_GETTEXT = 2182;
constexpr UINT SCI_GETCODEPAGE = 2137;
constexpr UINT SCI_SETFOCUS = 2380;
constexpr UINT SCI_GETFIRSTVISIBLELINE = 2152;
constexpr UINT SCI_GETLINECOUNT = 2154;
constexpr UINT SCI_LINESONSCREEN = 2370;
constexpr UINT SCI_LINESCROLL = 2168;
constexpr int SC_CP_UTF8 = 65001;

constexpr int STATUSBAR_TYPING_MODE = 5;

struct NppData {
    HWND _nppHandle{};
    HWND _scintillaMainHandle{};
    HWND _scintillaSecondHandle{};
};

struct ShortcutKey {
    bool _isCtrl{};
    bool _isAlt{};
    bool _isShift{};
    UCHAR _key{};
};

using PFUNCPLUGINCMD = void (*)();

struct FuncItem {
    wchar_t _itemName[64]{};
    PFUNCPLUGINCMD _pFunc{};
    int _cmdID{};
    bool _init2Check{};
    ShortcutKey* _pShKey{};
};

struct SCNotification {
    NMHDR nmhdr{};
    int position{};
    int ch{};
    int modifiers{};
    int modificationType{};
    const char* text{};
    int length{};
    int linesAdded{};
    int message{};
    WPARAM wParam{};
    LPARAM lParam{};
    int line{};
    int foldLevelNow{};
    int foldLevelPrev{};
    int margin{};
    int listType{};
    int x{};
    int y{};
    int token{};
    int annotationLinesAdded{};
    int updated{};
    int listCompletionMethod{};
};

struct ToolbarIconsWithDarkMode {
    HBITMAP hToolbarBmp{};
    HICON hToolbarIcon{};
    HICON hToolbarIconDarkMode{};
};

HWND CurrentScintilla(const NppData& data);
std::wstring CurrentFilePath(HWND nppHandle);
std::wstring PluginConfigDirectory(HWND nppHandle);
std::string CurrentScintillaText(HWND scintilla);
bool CurrentScintillaIsUtf8(HWND scintilla);
void SetStatus(HWND nppHandle, const std::wstring& text);
void SetMenuChecked(HWND nppHandle, int commandId, bool checked);
double ScintillaViewportRatio(HWND scintilla);
void SetScintillaViewportRatio(HWND scintilla, double ratio);

}  // namespace nmf::npp
