#pragma once

#include <string>
#include <windows.h>

namespace nmf::npp {

constexpr UINT NPPMSG = WM_USER + 1000;
constexpr UINT NPPM_GETCURRENTSCINTILLA = 2028;
constexpr UINT NPPM_GETCURRENTLANGTYPE = 2029;
constexpr UINT NPPM_GETFULLCURRENTPATH = 4025;
constexpr UINT NPPM_GETFILENAME = 4029;
constexpr UINT NPPM_GETPLUGINSCONFIGDIR = 2070;
constexpr UINT NPPM_SETSTATUSBAR = 2048;
constexpr UINT NPPM_SETMENUITEMCHECK = 2064;
constexpr UINT NPPM_ADDTOOLBARICON_FORDARKMODE = 2125;
constexpr UINT NPPM_DMMSHOW = 2054;
constexpr UINT NPPM_DMMHIDE = 2055;
constexpr UINT NPPM_DMMUPDATEDISPINFO = 2056;
constexpr UINT NPPM_DMMREGASDCKDLG = 2057;
constexpr UINT NPPM_ISDARKMODEENABLED = 2131;
constexpr UINT NPPM_DARKMODESUBCLASSANDTHEME = 2136;

// NppDarkMode::dmfInit / dmfHandleChange for NPPM_DARKMODESUBCLASSANDTHEME.
constexpr WPARAM kDarkModeSubclassInit = 0x0000000B;
constexpr WPARAM kDarkModeSubclassHandleChange = 0x0000000C;

// Docking manager (tTbData equivalent).
constexpr UINT DWS_ICONTAB = 0x00000001;
constexpr UINT DWS_DF_CONT_LEFT = 0u << 28;
constexpr UINT DWS_DF_CONT_RIGHT = 1u << 28;
constexpr UINT DWS_DF_CONT_TOP = 2u << 28;
constexpr UINT DWS_DF_CONT_BOTTOM = 3u << 28;
constexpr UINT DMN_FIRST = 1050;
constexpr UINT DMN_CLOSE = DMN_FIRST + 1;

struct DockingData {
    HWND hClient{};
    const wchar_t* pszName{};
    int dlgID{};
    UINT uMask{};
    HICON hIconTab{};
    const wchar_t* pszAddInfo{};
    RECT rcFloat{};
    int iPrevCont{};
    const wchar_t* pszModuleName{};
};

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
constexpr UINT SCI_GRABFOCUS = 2400;
constexpr UINT SCI_GETFIRSTVISIBLELINE = 2152;
constexpr UINT SCI_GETLINECOUNT = 2154;
constexpr UINT SCI_LINESONSCREEN = 2370;
constexpr UINT SCI_LINESCROLL = 2168;
constexpr UINT SCI_VISIBLEFROMDOCLINE = 2220;
constexpr UINT SCI_DOCLINEFROMVISIBLE = 2221;
constexpr UINT SCI_GOTOLINE = 2024;
constexpr UINT SCI_ENSUREVISIBLE = 2232;
constexpr int SC_CP_UTF8 = 65001;

constexpr UINT SCN_MODIFIED = 2008;
constexpr UINT SCN_CHARADDED = 2001;
constexpr int SC_MOD_INSERTTEXT = 0x1;
constexpr int SC_MOD_DELETETEXT = 0x2;

constexpr UINT SCI_GETCURRENTPOS = 2008;
constexpr UINT SCI_GOTOPOS = 2025;
constexpr UINT SCI_GETEOLMODE = 2030;
constexpr UINT SCI_BEGINUNDOACTION = 2078;
constexpr UINT SCI_ENDUNDOACTION = 2079;
constexpr UINT SCI_GETLINEENDPOSITION = 2136;
constexpr UINT SCI_GETSELECTIONSTART = 2143;
constexpr UINT SCI_GETSELECTIONEND = 2145;
constexpr UINT SCI_GETLINE = 2153;
constexpr UINT SCI_LINEFROMPOSITION = 2166;
constexpr UINT SCI_POSITIONFROMLINE = 2167;
constexpr UINT SCI_SETTARGETSTART = 2190;
constexpr UINT SCI_SETTARGETEND = 2192;
constexpr UINT SCI_REPLACETARGET = 2194;
constexpr UINT SCI_LINELENGTH = 2350;
constexpr UINT SCI_GETSELECTIONS = 2570;
constexpr UINT SCI_SETSEL = 2160;
constexpr int SC_EOL_CRLF = 0;

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

// Matches Scintilla's 64-bit SCNotification layout (Sci_Position fields).
using SciPosition = ptrdiff_t;

struct SCNotification {
    NMHDR nmhdr{};
    SciPosition position{};
    int ch{};
    int modifiers{};
    int modificationType{};
    const char* text{};
    SciPosition length{};
    SciPosition linesAdded{};
    int message{};
    WPARAM wParam{};
    LPARAM lParam{};
    SciPosition line{};
    int foldLevelNow{};
    int foldLevelPrev{};
    int margin{};
    int listType{};
    int x{};
    int y{};
    int token{};
    SciPosition annotationLinesAdded{};
    int updated{};
    int listCompletionMethod{};
    int characterSource{};
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
int ScintillaFirstVisibleLine(HWND scintilla);
double ScintillaViewportRatio(HWND scintilla);
void SetScintillaViewportRatio(HWND scintilla, double ratio);
void SetScintillaFirstVisibleLine(HWND scintilla, int line);
std::string ScintillaLineText(HWND scintilla, int line);
void ScintillaReplaceRange(HWND scintilla, ptrdiff_t start, ptrdiff_t end, const std::string& text);

}  // namespace nmf::npp
