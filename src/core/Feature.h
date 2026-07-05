#pragma once

#include "core/Settings.h"

#include <string>
#include <windows.h>

namespace nmf {

struct HostContext {
    HWND npp{};
    HWND scintillaMain{};
    HWND scintillaSecond{};
};

enum class PluginCommand {
    ToggleRenderedView,
    RefreshRenderedView,
    Settings,
    About,
};

struct ActiveDocument {
    HWND scintilla{};
    std::wstring path;
    bool isUtf8{true};
};

class Feature {
public:
    virtual ~Feature() = default;
    virtual std::wstring Id() const = 0;
    virtual std::wstring DisplayName() const = 0;
    virtual void LoadSettings(const AppSettings& settings) = 0;
    virtual void SaveSettings(AppSettings& settings) const = 0;
    virtual void OnCommand(PluginCommand command, const ActiveDocument& document) = 0;
    virtual void OnDocumentChanged(const ActiveDocument& document) = 0;
    virtual void OnFileSaved(const ActiveDocument& document) = 0;
};

}  // namespace nmf
