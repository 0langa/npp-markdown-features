#include "core/FeatureRegistry.h"
#include "core/MarkdownViewFeature.h"
#include "core/SettingsStore.h"
#include "core/Strings.h"
#include "plugin/NppApi.h"
#include "rendering/WebViewHost.h"
#include "ui/AboutDialog.h"
#include "ui/SettingsDialog.h"
#include "ui/ToolbarIcon.h"

#include <array>
#include <filesystem>
#include <memory>
#include <string>

namespace {

using nmf::PluginCommand;

constexpr wchar_t kPluginName[] = L"Markdown Features";
constexpr int kCommandCount = 4;
constexpr int kToggleIndex = 0;
constexpr int kRefreshIndex = 1;
constexpr int kSettingsIndex = 2;
constexpr int kAboutIndex = 3;

nmf::npp::NppData g_nppData{};
std::array<nmf::npp::FuncItem, kCommandCount> g_funcItems{};
std::unique_ptr<nmf::SettingsStore> g_settingsStore;
nmf::AppSettings g_settings;
nmf::FeatureRegistry g_features;
nmf::MarkdownViewFeature* g_markdownFeature = nullptr;
nmf::WebViewHost g_webView;
HICON g_lightIcon = nullptr;
HICON g_darkIcon = nullptr;
bool g_initialized = false;

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

void EnsureInitialized() {
    if (g_initialized || g_nppData._nppHandle == nullptr) {
        return;
    }
    g_settingsStore = std::make_unique<nmf::SettingsStore>(SettingsPath());
    g_settings = g_settingsStore->Load();

    auto markdownFeature = std::make_unique<nmf::MarkdownViewFeature>(
        ReadDocumentText,
        [](HWND scintilla, const std::string& html, const std::wstring& sourcePath) {
            g_webView.ShowHtml(scintilla, html, sourcePath);
        },
        []() {
            g_webView.Hide();
        },
        [](const std::wstring& status) {
            nmf::npp::SetStatus(g_nppData._nppHandle, status);
        });
    g_markdownFeature = markdownFeature.get();
    g_features.Add(std::move(markdownFeature));
    g_features.LoadSettings(g_settings);
    g_initialized = true;
    g_features.NotifyDocumentChanged(ActiveDocument());
    UpdateToggleCheck();
}

void ToggleRenderedView() {
    EnsureInitialized();
    g_features.DispatchCommand(PluginCommand::ToggleRenderedView, ActiveDocument());
    UpdateToggleCheck();
    SaveSettings();
}

void RefreshRenderedView() {
    EnsureInitialized();
    g_features.DispatchCommand(PluginCommand::RefreshRenderedView, ActiveDocument());
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

void OpenAbout() {
    nmf::ShowAboutDialog(g_nppData._nppHandle);
}

void SetCommand(int index, const wchar_t* name, nmf::npp::PFUNCPLUGINCMD callback, bool checked = false) {
    wcsncpy_s(g_funcItems[index]._itemName, name, _TRUNCATE);
    g_funcItems[index]._pFunc = callback;
    g_funcItems[index]._cmdID = 0;
    g_funcItems[index]._init2Check = checked;
    g_funcItems[index]._pShKey = nullptr;
}

void RegisterCommands() {
    static bool registered = false;
    if (registered) {
        return;
    }
    SetCommand(kToggleIndex, L"Toggle Rendered View", ToggleRenderedView);
    SetCommand(kRefreshIndex, L"Refresh Rendered View", RefreshRenderedView);
    SetCommand(kSettingsIndex, L"Settings...", OpenSettings);
    SetCommand(kAboutIndex, L"About", OpenAbout);
    registered = true;
}

void AddToolbarIcon() {
    if (g_lightIcon == nullptr) {
        g_lightIcon = nmf::CreateToolbarIcon(false);
    }
    if (g_darkIcon == nullptr) {
        g_darkIcon = nmf::CreateToolbarIcon(true);
    }
    nmf::npp::ToolbarIconsWithDarkMode icons{};
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
}

void NotifyFileSaved() {
    EnsureInitialized();
    g_features.NotifyFileSaved(ActiveDocument());
    SaveSettings();
}

void Shutdown() {
    SaveSettings();
    g_webView.Destroy();
    if (g_lightIcon != nullptr) {
        ::DestroyIcon(g_lightIcon);
        g_lightIcon = nullptr;
    }
    if (g_darkIcon != nullptr) {
        ::DestroyIcon(g_darkIcon);
        g_darkIcon = nullptr;
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
        case nmf::npp::NPPN_SHUTDOWN:
            Shutdown();
            break;
        default:
            break;
    }
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) {
    return TRUE;
}
