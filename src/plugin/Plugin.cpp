#include "core/FeatureRegistry.h"
#include "core/MarkdownViewFeature.h"
#include "core/SettingsStore.h"
#include "core/Strings.h"
#include "plugin/FormattingController.h"
#include "plugin/LinkController.h"
#include "plugin/ListEditingController.h"
#include "plugin/NppApi.h"
#include "plugin/TableEditingController.h"
#include "rendering/WebViewHost.h"
#include "ui/OutlinePanel.h"
#include "ui/SettingsDialog.h"
#include "ui/ToolbarIcon.h"

#include <algorithm>
#include <array>
#include <commctrl.h>
#include <filesystem>
#include <memory>
#include <string>

namespace {

using nmf::PluginCommand;

constexpr wchar_t kPluginName[] = L"Markdown Features";
constexpr int kCommandCount = 22;
constexpr int kToggleIndex = 0;
constexpr int kOutlineIndex = 1;
constexpr int kCheckboxIndex = 2;
constexpr int kRenumberIndex = 3;
constexpr int kFormatTableIndex = 4;
constexpr int kInsertColumnIndex = 5;
constexpr int kDeleteColumnIndex = 6;
constexpr int kCycleAlignmentIndex = 7;
constexpr int kSmartTypingIndex = 8;
constexpr int kSettingsIndex = 9;
constexpr int kBoldIndex = 10;
constexpr int kItalicIndex = 11;
constexpr int kStrikethroughIndex = 12;
constexpr int kInlineCodeIndex = 13;
constexpr int kHeadingUpIndex = 14;
constexpr int kHeadingDownIndex = 15;
constexpr int kBlockquoteIndex = 16;
constexpr int kCodeFenceIndex = 17;
constexpr int kInsertLinkIndex = 18;
constexpr int kInsertImageIndex = 19;
constexpr int kFollowLinkIndex = 20;
constexpr int kCheckLinksIndex = 21;

nmf::npp::NppData g_nppData{};
std::array<nmf::npp::FuncItem, kCommandCount> g_funcItems{};
std::unique_ptr<nmf::SettingsStore> g_settingsStore;
nmf::AppSettings g_settings;
nmf::FeatureRegistry g_features;
nmf::MarkdownViewFeature* g_markdownFeature = nullptr;
nmf::WebViewHost g_webView;
nmf::OutlinePanel g_outlinePanel;
nmf::ListEditingController g_listEditing;
nmf::TableEditingController g_tableEditing;
nmf::FormattingController g_formatting;
nmf::LinkController g_links;
UINT_PTR g_outlineTimer = 0;
HICON g_lightIcon = nullptr;
HICON g_darkIcon = nullptr;
HBITMAP g_toolbarBitmap = nullptr;
bool g_initialized = false;
bool g_scintillaSubclassed = false;
bool g_swallowNextTabChar = false;
constexpr UINT_PTR kScintillaSubclassId = 0x4D46;  // 'MF'

nmf::ActiveDocument ActiveDocument() {
    HWND scintilla = nmf::npp::CurrentScintilla(g_nppData);
    return {scintilla, nmf::npp::CurrentFilePath(g_nppData._nppHandle), nmf::npp::CurrentScintillaIsUtf8(scintilla)};
}

std::string ReadDocumentText(const nmf::ActiveDocument& document) {
    auto bytes = nmf::npp::CurrentScintillaText(document.scintilla);
    if (document.isUtf8) {
        return bytes;
    }
    const int wideSize = MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (wideSize <= 0) {
        return bytes;
    }
    std::wstring wide(static_cast<size_t>(wideSize), L'\0');
    MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), wide.data(), wideSize);
    return nmf::WideToUtf8(wide);
}

void UpdateToggleCheck() {
    if (g_markdownFeature != nullptr) {
        nmf::npp::SetMenuChecked(g_nppData._nppHandle, g_funcItems[kToggleIndex]._cmdID, g_markdownFeature->IsRenderedMode());
    }
}

std::filesystem::path SettingsPath() {
    auto configRoot = nmf::npp::PluginConfigDirectory(g_nppData._nppHandle);
    if (configRoot.empty()) {
        wchar_t temp[MAX_PATH]{};
        ::GetTempPath(MAX_PATH, temp);
        configRoot = temp;
    }
    return std::filesystem::path(configRoot) / L"NppMarkdownFeatures" / L"settings.json";
}

void SaveSettings() {
    if (g_settingsStore && g_markdownFeature) {
        g_markdownFeature->SaveSettings(g_settings);
        g_settingsStore->Save(g_settings);
    }
}

bool ActiveDocumentIsMarkdown(const nmf::ActiveDocument& document) {
    return g_markdownFeature != nullptr && nmf::EndsWithMarkdownExtension(document.path, g_markdownFeature->Settings().extensions);
}

void UpdateOutlineNow() {
    if (!g_outlinePanel.IsCreated() || !g_outlinePanel.IsVisible()) {
        return;
    }
    const auto document = ActiveDocument();
    const bool isMarkdown = ActiveDocumentIsMarkdown(document) && document.scintilla != nullptr;
    nmf::MarkdownOutline outline;
    if (isMarkdown) {
        outline = nmf::MarkdownOutline::Parse(ReadDocumentText(document));
    }
    const auto name = document.path.empty() ? std::wstring{} : std::filesystem::path(document.path).filename().wstring();
    g_outlinePanel.SetOutline(outline, name, isMarkdown);
}

void CALLBACK OutlineTimerProc(HWND, UINT, UINT_PTR id, DWORD) {
    ::KillTimer(nullptr, id);
    if (id == g_outlineTimer) {
        g_outlineTimer = 0;
        UpdateOutlineNow();
    }
}

void ScheduleOutlineUpdate() {
    if (!g_outlinePanel.IsCreated() || !g_outlinePanel.IsVisible()) {
        return;
    }
    if (g_outlineTimer != 0) {
        ::KillTimer(nullptr, g_outlineTimer);
    }
    g_outlineTimer = ::SetTimer(nullptr, 0, 350, OutlineTimerProc);
}

// Intercepts Tab/Shift+Tab ahead of Scintilla for table cell navigation.
// Scintilla's own Tab handling goes through SCI_TAB (no SCN_CHARADDED), so a
// notification-based approach cannot see it.
LRESULT CALLBACK ScintillaSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
    if (message == WM_KEYDOWN) {
        if (wParam == VK_TAB && g_initialized && g_tableEditing.SmartTabEnabled()) {
            const bool ctrl = (::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            if (!ctrl && !alt) {
                const auto document = ActiveDocument();
                if (document.scintilla == hwnd && ActiveDocumentIsMarkdown(document)) {
                    const bool backward = (::GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                    if (g_tableEditing.NavigateCell(hwnd, !backward)) {
                        // The already-translated WM_CHAR '\t' still arrives; drop it.
                        g_swallowNextTabChar = true;
                        return 0;
                    }
                }
            }
        }
        if (wParam == 'V' && g_initialized && (::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0
            && (::GetAsyncKeyState(VK_MENU) & 0x8000) == 0 && (::GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0) {
            const auto document = ActiveDocument();
            if (document.scintilla == hwnd && ActiveDocumentIsMarkdown(document) && g_links.HandlePaste(hwnd)) {
                return 0;
            }
        }
        if (wParam != VK_TAB) {
            g_swallowNextTabChar = false;
        }
    } else if (message == WM_CHAR) {
        if (g_swallowNextTabChar && wParam == '\t') {
            g_swallowNextTabChar = false;
            return 0;
        }
        g_swallowNextTabChar = false;
    }
    return ::DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnsureScintillaSubclassed() {
    if (g_scintillaSubclassed) {
        return;
    }
    if (g_nppData._scintillaMainHandle != nullptr) {
        ::SetWindowSubclass(g_nppData._scintillaMainHandle, ScintillaSubclassProc, kScintillaSubclassId, 0);
    }
    if (g_nppData._scintillaSecondHandle != nullptr) {
        ::SetWindowSubclass(g_nppData._scintillaSecondHandle, ScintillaSubclassProc, kScintillaSubclassId, 0);
    }
    g_scintillaSubclassed = true;
}

void RemoveScintillaSubclass() {
    if (!g_scintillaSubclassed) {
        return;
    }
    if (g_nppData._scintillaMainHandle != nullptr) {
        ::RemoveWindowSubclass(g_nppData._scintillaMainHandle, ScintillaSubclassProc, kScintillaSubclassId);
    }
    if (g_nppData._scintillaSecondHandle != nullptr) {
        ::RemoveWindowSubclass(g_nppData._scintillaSecondHandle, ScintillaSubclassProc, kScintillaSubclassId);
    }
    g_scintillaSubclassed = false;
}

void EnsureInitialized() {
    if (g_initialized || g_nppData._nppHandle == nullptr) {
        return;
    }
    g_settingsStore = std::make_unique<nmf::SettingsStore>(SettingsPath());
    g_settings = g_settingsStore->Load();

    auto markdownFeature = std::make_unique<nmf::MarkdownViewFeature>(
        ReadDocumentText,
        [](const nmf::ActiveDocument& document) {
            return nmf::npp::ScintillaViewportRatio(document.scintilla);
        },
        [](const nmf::ActiveDocument&) {
            return g_webView.LastScrollTarget();
        },
        [](const nmf::ActiveDocument& document) {
            return nmf::npp::ScintillaFirstVisibleLine(document.scintilla);
        },
        [](HWND scintilla, const std::string& html, const std::wstring& sourcePath, const nmf::ScrollTarget& target) {
            g_webView.ShowHtml(scintilla, html, sourcePath, target);
        },
        []() {
            return g_webView.Hide();
        },
        [](const nmf::ActiveDocument& document, const nmf::ScrollTarget& target, const nmf::MarkdownOutline& outline) {
            const nmf::BlockScrollSyncStrategy sync;
            const int docLine = sync.RenderedToRawLine(target, outline);
            if (docLine >= 0) {
                nmf::npp::SetScintillaFirstVisibleLine(document.scintilla, docLine);
                return;
            }
            nmf::npp::SetScintillaViewportRatio(document.scintilla, target.ratio);
        },
        [](const std::wstring& status) {
            nmf::npp::SetStatus(g_nppData._nppHandle, status);
        });
    g_markdownFeature = markdownFeature.get();
    g_features.Add(std::move(markdownFeature));
    g_features.LoadSettings(g_settings);

    g_outlinePanel.Initialize(g_nppData._nppHandle, kOutlineIndex);
    g_outlinePanel.SetJumpCallback([](int line) {
        const auto document = ActiveDocument();
        if (document.scintilla == nullptr) {
            return;
        }
        ::SendMessage(document.scintilla, nmf::npp::SCI_ENSUREVISIBLE, line, 0);
        ::SendMessage(document.scintilla, nmf::npp::SCI_GOTOLINE, line, 0);
        nmf::npp::SetScintillaFirstVisibleLine(document.scintilla, line);
        if (g_markdownFeature != nullptr && g_markdownFeature->IsRenderedMode() && ActiveDocumentIsMarkdown(document)) {
            g_webView.ScrollToSourceLine(static_cast<double>(line) + 1.0);
        } else {
            ::SendMessage(document.scintilla, nmf::npp::SCI_GRABFOCUS, 0, 0);
        }
    });
    g_outlinePanel.SetVisibilityCallback([](bool visible) {
        g_settings.outline.visible = visible;
        nmf::npp::SetMenuChecked(g_nppData._nppHandle, g_funcItems[kOutlineIndex]._cmdID, visible);
        SaveSettings();
    });

    EnsureScintillaSubclassed();
    g_listEditing.SetSmartEnterEnabled(g_settings.listEditing.smartEnter);
    g_tableEditing.SetSmartTabEnabled(g_settings.tableEditing.smartTab);
    g_links.SetPasteUrlEnabled(g_settings.linkTools.pasteUrlAsLink);
    nmf::npp::SetMenuChecked(g_nppData._nppHandle, g_funcItems[kSmartTypingIndex]._cmdID, g_settings.listEditing.smartEnter);

    g_initialized = true;
    g_features.NotifyDocumentChanged(ActiveDocument());
    UpdateToggleCheck();

    if (g_settings.outline.visible) {
        g_outlinePanel.Show();
        UpdateOutlineNow();
    }
}

void ToggleRenderedView() {
    EnsureInitialized();
    g_features.DispatchCommand(PluginCommand::ToggleRenderedView, ActiveDocument());
    UpdateToggleCheck();
    SaveSettings();
}

void ToggleOutlinePanel() {
    EnsureInitialized();
    if (g_outlinePanel.IsVisible()) {
        g_outlinePanel.Hide();
    } else {
        g_outlinePanel.Show();
        UpdateOutlineNow();
    }
}

void ToggleTaskCheckbox() {
    EnsureInitialized();
    const auto document = ActiveDocument();
    if (ActiveDocumentIsMarkdown(document)) {
        g_listEditing.ToggleCheckbox(document.scintilla);
    }
}

void RenumberList() {
    EnsureInitialized();
    const auto document = ActiveDocument();
    if (ActiveDocumentIsMarkdown(document)) {
        g_listEditing.RenumberAtCaret(document.scintilla);
    }
}

void ToggleSmartTyping() {
    EnsureInitialized();
    const bool enabled = !g_listEditing.SmartEnterEnabled();
    g_listEditing.SetSmartEnterEnabled(enabled);
    g_tableEditing.SetSmartTabEnabled(enabled);
    g_links.SetPasteUrlEnabled(enabled);
    g_settings.listEditing.smartEnter = enabled;
    g_settings.tableEditing.smartTab = enabled;
    g_settings.linkTools.pasteUrlAsLink = enabled;
    nmf::npp::SetMenuChecked(g_nppData._nppHandle, g_funcItems[kSmartTypingIndex]._cmdID, enabled);
    SaveSettings();
}

void FormatTable() {
    EnsureInitialized();
    const auto document = ActiveDocument();
    if (ActiveDocumentIsMarkdown(document)) {
        g_tableEditing.FormatTableAtCaret(document.scintilla);
    }
}

void TableInsertColumn() {
    EnsureInitialized();
    const auto document = ActiveDocument();
    if (ActiveDocumentIsMarkdown(document)) {
        g_tableEditing.InsertColumn(document.scintilla);
    }
}

void TableDeleteColumn() {
    EnsureInitialized();
    const auto document = ActiveDocument();
    if (ActiveDocumentIsMarkdown(document)) {
        g_tableEditing.DeleteColumn(document.scintilla);
    }
}

void TableCycleAlignment() {
    EnsureInitialized();
    const auto document = ActiveDocument();
    if (ActiveDocumentIsMarkdown(document)) {
        g_tableEditing.CycleColumnAlignment(document.scintilla);
    }
}

void WithMarkdownDocument(void (*action)(HWND)) {
    EnsureInitialized();
    const auto document = ActiveDocument();
    if (ActiveDocumentIsMarkdown(document)) {
        action(document.scintilla);
    }
}

void ToggleBold() {
    WithMarkdownDocument([](HWND scintilla) { g_formatting.ToggleInlineMarker(scintilla, "**"); });
}

void ToggleItalic() {
    WithMarkdownDocument([](HWND scintilla) { g_formatting.ToggleInlineMarker(scintilla, "*"); });
}

void ToggleStrikethrough() {
    WithMarkdownDocument([](HWND scintilla) { g_formatting.ToggleInlineMarker(scintilla, "~~"); });
}

void ToggleInlineCode() {
    WithMarkdownDocument([](HWND scintilla) { g_formatting.ToggleInlineMarker(scintilla, "`"); });
}

void HeadingLevelUp() {
    WithMarkdownDocument([](HWND scintilla) { g_formatting.ChangeHeading(scintilla, 1); });
}

void HeadingLevelDown() {
    WithMarkdownDocument([](HWND scintilla) { g_formatting.ChangeHeading(scintilla, -1); });
}

void ToggleBlockquoteCommand() {
    WithMarkdownDocument([](HWND scintilla) { g_formatting.ToggleBlockquote(scintilla); });
}

void InsertCodeFenceCommand() {
    WithMarkdownDocument([](HWND scintilla) { g_formatting.InsertCodeFence(scintilla); });
}

void InsertLinkCommand() {
    WithMarkdownDocument([](HWND scintilla) { g_links.InsertLink(scintilla, false); });
}

void InsertImageCommand() {
    WithMarkdownDocument([](HWND scintilla) { g_links.InsertLink(scintilla, true); });
}

void FollowLinkCommand() {
    EnsureInitialized();
    const auto document = ActiveDocument();
    if (ActiveDocumentIsMarkdown(document)) {
        const auto status = g_links.FollowLink(document.scintilla, g_nppData._nppHandle, document.path);
        nmf::npp::SetStatus(g_nppData._nppHandle, status);
    }
}

void CheckLinksCommand() {
    EnsureInitialized();
    const auto document = ActiveDocument();
    if (!ActiveDocumentIsMarkdown(document)) {
        return;
    }
    const auto result = g_links.CheckLinks(document.scintilla, document.path);
    if (!result.reportUtf8.empty()) {
        ::SendMessage(g_nppData._nppHandle, WM_COMMAND, nmf::npp::IDM_FILE_NEW, 0);
        const auto reportDocument = ActiveDocument();
        if (reportDocument.scintilla != nullptr) {
            ::SendMessage(reportDocument.scintilla, nmf::npp::SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(result.reportUtf8.c_str()));
        }
    }
    nmf::npp::SetStatus(g_nppData._nppHandle, result.status);
}

void OpenSettings() {
    EnsureInitialized();
    if (g_markdownFeature == nullptr || g_settingsStore == nullptr) {
        return;
    }
    auto settings = g_markdownFeature->Settings();
    if (nmf::ShowSettingsDialog(g_nppData._nppHandle, settings, g_settingsStore->Path())) {
        g_markdownFeature->UpdateSettings(settings);
        g_features.DispatchCommand(PluginCommand::RefreshRenderedView, ActiveDocument());
        UpdateToggleCheck();
        SaveSettings();
    }
}

void SetCommand(int index, const wchar_t* name, nmf::npp::PFUNCPLUGINCMD callback, nmf::npp::ShortcutKey* shortcut = nullptr, bool checked = false) {
    wcsncpy_s(g_funcItems[index]._itemName, name, _TRUNCATE);
    g_funcItems[index]._pFunc = callback;
    g_funcItems[index]._cmdID = 0;
    g_funcItems[index]._init2Check = checked;
    g_funcItems[index]._pShKey = shortcut;
}

void RegisterCommands() {
    static bool registered = false;
    if (registered) {
        return;
    }
    static nmf::npp::ShortcutKey toggleShortcut{true, false, true, 'M'};
    static nmf::npp::ShortcutKey outlineShortcut{true, false, true, 'O'};
    static nmf::npp::ShortcutKey checkboxShortcut{true, false, true, 'X'};
    static nmf::npp::ShortcutKey formatTableShortcut{true, false, true, 'T'};
    SetCommand(kToggleIndex, L"Toggle Rendered View", ToggleRenderedView, &toggleShortcut);
    SetCommand(kOutlineIndex, L"Document Outline", ToggleOutlinePanel, &outlineShortcut);
    SetCommand(kCheckboxIndex, L"Toggle Task Checkbox", ToggleTaskCheckbox, &checkboxShortcut);
    SetCommand(kRenumberIndex, L"Renumber List", RenumberList);
    SetCommand(kFormatTableIndex, L"Format Table", FormatTable, &formatTableShortcut);
    SetCommand(kInsertColumnIndex, L"Table: Insert Column", TableInsertColumn);
    SetCommand(kDeleteColumnIndex, L"Table: Delete Column", TableDeleteColumn);
    SetCommand(kCycleAlignmentIndex, L"Table: Cycle Column Alignment", TableCycleAlignment);
    SetCommand(kSmartTypingIndex, L"Smart Typing (Lists, Tables)", ToggleSmartTyping, nullptr, true);
    SetCommand(kSettingsIndex, L"Settings...", OpenSettings);
    static nmf::npp::ShortcutKey boldShortcut{true, true, false, 'B'};
    static nmf::npp::ShortcutKey italicShortcut{true, true, false, 'I'};
    static nmf::npp::ShortcutKey strikethroughShortcut{true, true, false, 'U'};
    static nmf::npp::ShortcutKey inlineCodeShortcut{true, true, false, 'C'};
    static nmf::npp::ShortcutKey blockquoteShortcut{true, true, false, 'Q'};
    static nmf::npp::ShortcutKey codeFenceShortcut{true, true, false, 'F'};
    SetCommand(kBoldIndex, L"Bold", ToggleBold, &boldShortcut);
    SetCommand(kItalicIndex, L"Italic", ToggleItalic, &italicShortcut);
    SetCommand(kStrikethroughIndex, L"Strikethrough", ToggleStrikethrough, &strikethroughShortcut);
    SetCommand(kInlineCodeIndex, L"Inline Code", ToggleInlineCode, &inlineCodeShortcut);
    SetCommand(kHeadingUpIndex, L"Heading Level Up", HeadingLevelUp);
    SetCommand(kHeadingDownIndex, L"Heading Level Down", HeadingLevelDown);
    SetCommand(kBlockquoteIndex, L"Toggle Blockquote", ToggleBlockquoteCommand, &blockquoteShortcut);
    SetCommand(kCodeFenceIndex, L"Insert Code Fence", InsertCodeFenceCommand, &codeFenceShortcut);
    static nmf::npp::ShortcutKey insertLinkShortcut{true, true, false, 'L'};
    static nmf::npp::ShortcutKey followLinkShortcut{true, true, false, 'G'};
    SetCommand(kInsertLinkIndex, L"Insert Link", InsertLinkCommand, &insertLinkShortcut);
    SetCommand(kInsertImageIndex, L"Insert Image", InsertImageCommand);
    SetCommand(kFollowLinkIndex, L"Follow Link", FollowLinkCommand, &followLinkShortcut);
    SetCommand(kCheckLinksIndex, L"Check Links", CheckLinksCommand);
    registered = true;
}

void AddToolbarIcon() {
    if (g_toolbarBitmap == nullptr) {
        g_toolbarBitmap = nmf::CreateToolbarBitmap();
    }
    if (g_lightIcon == nullptr) {
        g_lightIcon = nmf::CreateToolbarIcon(false);
    }
    if (g_darkIcon == nullptr) {
        g_darkIcon = nmf::CreateToolbarIcon(true);
    }
    nmf::npp::ToolbarIconsWithDarkMode icons{};
    icons.hToolbarBmp = g_toolbarBitmap;
    icons.hToolbarIcon = g_lightIcon;
    icons.hToolbarIconDarkMode = g_darkIcon;
    ::SendMessage(
        g_nppData._nppHandle,
        nmf::npp::NPPM_ADDTOOLBARICON_FORDARKMODE,
        static_cast<WPARAM>(g_funcItems[kToggleIndex]._cmdID),
        reinterpret_cast<LPARAM>(&icons));
}

void NotifyDocumentChanged() {
    EnsureInitialized();
    g_features.NotifyDocumentChanged(ActiveDocument());
    UpdateToggleCheck();
    UpdateOutlineNow();
}

// Rebuild the flat 18-entry plugin menu into grouped submenus. Command IDs
// are preserved, so shortcuts and NPPM_SETMENUITEMCHECK keep working.
void ReorganizeMenu() {
    static bool done = false;
    if (done || g_nppData._nppHandle == nullptr) {
        return;
    }
    const auto pluginsMenu = reinterpret_cast<HMENU>(::SendMessage(g_nppData._nppHandle, nmf::npp::NPPM_GETMENUHANDLE, 0, 0));
    if (pluginsMenu == nullptr) {
        return;
    }
    HMENU ourMenu = nullptr;
    const int topCount = ::GetMenuItemCount(pluginsMenu);
    for (int index = 0; index < topCount; ++index) {
        wchar_t label[128]{};
        ::GetMenuString(pluginsMenu, static_cast<UINT>(index), label, 127, MF_BYPOSITION);
        std::wstring name(label);
        name.erase(std::remove(name.begin(), name.end(), L'&'), name.end());
        if (name == kPluginName) {
            ourMenu = ::GetSubMenu(pluginsMenu, index);
            break;
        }
    }
    if (ourMenu == nullptr) {
        return;
    }

    std::array<std::wstring, kCommandCount> labels;
    for (int index = 0; index < kCommandCount; ++index) {
        wchar_t label[128]{};
        ::GetMenuString(ourMenu, static_cast<UINT>(g_funcItems[index]._cmdID), label, 127, MF_BYCOMMAND);
        labels[static_cast<size_t>(index)] = label;
        if (labels[static_cast<size_t>(index)].empty()) {
            return;  // unexpected menu shape; keep the flat menu
        }
    }
    while (::GetMenuItemCount(ourMenu) > 0) {
        ::RemoveMenu(ourMenu, 0, MF_BYPOSITION);
    }

    const auto append = [&](HMENU menu, int index) {
        ::AppendMenu(menu, MF_STRING, static_cast<UINT_PTR>(g_funcItems[index]._cmdID), labels[static_cast<size_t>(index)].c_str());
    };
    append(ourMenu, kToggleIndex);
    append(ourMenu, kOutlineIndex);
    ::AppendMenu(ourMenu, MF_SEPARATOR, 0, nullptr);

    HMENU formatMenu = ::CreatePopupMenu();
    append(formatMenu, kBoldIndex);
    append(formatMenu, kItalicIndex);
    append(formatMenu, kStrikethroughIndex);
    append(formatMenu, kInlineCodeIndex);
    ::AppendMenu(formatMenu, MF_SEPARATOR, 0, nullptr);
    append(formatMenu, kHeadingUpIndex);
    append(formatMenu, kHeadingDownIndex);
    ::AppendMenu(formatMenu, MF_SEPARATOR, 0, nullptr);
    append(formatMenu, kBlockquoteIndex);
    append(formatMenu, kCodeFenceIndex);
    ::AppendMenu(ourMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(formatMenu), L"Format");

    HMENU listsMenu = ::CreatePopupMenu();
    append(listsMenu, kCheckboxIndex);
    append(listsMenu, kRenumberIndex);
    ::AppendMenu(ourMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(listsMenu), L"Lists");

    HMENU tableMenu = ::CreatePopupMenu();
    append(tableMenu, kFormatTableIndex);
    append(tableMenu, kInsertColumnIndex);
    append(tableMenu, kDeleteColumnIndex);
    append(tableMenu, kCycleAlignmentIndex);
    ::AppendMenu(ourMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(tableMenu), L"Table");

    HMENU linksMenu = ::CreatePopupMenu();
    append(linksMenu, kInsertLinkIndex);
    append(linksMenu, kInsertImageIndex);
    ::AppendMenu(linksMenu, MF_SEPARATOR, 0, nullptr);
    append(linksMenu, kFollowLinkIndex);
    append(linksMenu, kCheckLinksIndex);
    ::AppendMenu(ourMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(linksMenu), L"Links");

    ::AppendMenu(ourMenu, MF_SEPARATOR, 0, nullptr);
    append(ourMenu, kSmartTypingIndex);
    append(ourMenu, kSettingsIndex);

    // Recreate lost check states.
    UpdateToggleCheck();
    nmf::npp::SetMenuChecked(g_nppData._nppHandle, g_funcItems[kSmartTypingIndex]._cmdID, g_settings.listEditing.smartEnter);
    nmf::npp::SetMenuChecked(g_nppData._nppHandle, g_funcItems[kOutlineIndex]._cmdID, g_outlinePanel.IsVisible());
    done = true;
}

void NotifyFileSaved() {
    EnsureInitialized();
    g_features.NotifyFileSaved(ActiveDocument());
    SaveSettings();
}

void Shutdown() {
    SaveSettings();
    RemoveScintillaSubclass();
    if (g_outlineTimer != 0) {
        ::KillTimer(nullptr, g_outlineTimer);
        g_outlineTimer = 0;
    }
    g_outlinePanel.Destroy();
    g_webView.Destroy();
    if (g_lightIcon != nullptr) {
        ::DestroyIcon(g_lightIcon);
        g_lightIcon = nullptr;
    }
    if (g_darkIcon != nullptr) {
        ::DestroyIcon(g_darkIcon);
        g_darkIcon = nullptr;
    }
    if (g_toolbarBitmap != nullptr) {
        ::DeleteObject(g_toolbarBitmap);
        g_toolbarBitmap = nullptr;
    }
    g_markdownFeature = nullptr;
    g_settingsStore.reset();
}

}  // namespace

extern "C" __declspec(dllexport) BOOL isUnicode() {
    return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(nmf::npp::NppData notpadPlusData) {
    g_nppData = notpadPlusData;
    RegisterCommands();
}

extern "C" __declspec(dllexport) const wchar_t* getName() {
    return kPluginName;
}

extern "C" __declspec(dllexport) nmf::npp::FuncItem* getFuncsArray(int* nbF) {
    RegisterCommands();
    *nbF = kCommandCount;
    return g_funcItems.data();
}

extern "C" __declspec(dllexport) void beNotified(nmf::npp::SCNotification* notifyCode) {
    if (notifyCode == nullptr) {
        return;
    }
    switch (notifyCode->nmhdr.code) {
        case nmf::npp::NPPN_READY:
            EnsureInitialized();
            ReorganizeMenu();
            break;
        case nmf::npp::NPPN_TBMODIFICATION:
            AddToolbarIcon();
            break;
        case nmf::npp::NPPN_BUFFERACTIVATED:
        case nmf::npp::NPPN_LANGCHANGED:
        case nmf::npp::NPPN_FILEOPENED:
        case nmf::npp::NPPN_FILECLOSED:
            NotifyDocumentChanged();
            break;
        case nmf::npp::NPPN_FILESAVED:
            NotifyFileSaved();
            break;
        case nmf::npp::NPPN_DARKMODECHANGED:
            g_outlinePanel.HandleDarkModeChanged();
            break;
        case nmf::npp::NPPN_SHUTDOWN:
            Shutdown();
            break;
        case nmf::npp::SCN_MODIFIED:
            if ((notifyCode->nmhdr.hwndFrom == g_nppData._scintillaMainHandle || notifyCode->nmhdr.hwndFrom == g_nppData._scintillaSecondHandle)
                && (notifyCode->modificationType & (nmf::npp::SC_MOD_INSERTTEXT | nmf::npp::SC_MOD_DELETETEXT)) != 0) {
                ScheduleOutlineUpdate();
            }
            break;
        case nmf::npp::SCN_CHARADDED:
            if (g_initialized
                && (notifyCode->nmhdr.hwndFrom == g_nppData._scintillaMainHandle || notifyCode->nmhdr.hwndFrom == g_nppData._scintillaSecondHandle)) {
                const auto document = ActiveDocument();
                if (ActiveDocumentIsMarkdown(document) && document.scintilla == notifyCode->nmhdr.hwndFrom) {
                    g_listEditing.OnCharAdded(document.scintilla, notifyCode->ch);
                }
            }
            break;
        default:
            break;
    }
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) {
    return TRUE;
}
