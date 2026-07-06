#pragma once

#include <string>
#include <vector>

namespace nmf {

enum class DisplayMode {
    Raw,
    Rendered,
};

struct MarkdownViewSettings {
    DisplayMode defaultMode{DisplayMode::Raw};
    std::wstring toggleScope{L"global"};
    std::wstring refreshMode{L"onSave"};
    std::vector<std::wstring> extensions{L".md", L".markdown"};
};

struct OutlineSettings {
    bool visible{false};
};

struct ListEditingSettings {
    bool smartEnter{true};
};

struct TableEditingSettings {
    bool smartTab{true};
};

struct LinkToolsSettings {
    bool pasteUrlAsLink{true};
};

struct AppSettings {
    int schemaVersion{1};
    MarkdownViewSettings markdownView{};
    OutlineSettings outline{};
    ListEditingSettings listEditing{};
    TableEditingSettings tableEditing{};
    LinkToolsSettings linkTools{};
};

std::wstring ToString(DisplayMode mode);
DisplayMode DisplayModeFromString(const std::wstring& value);

}  // namespace nmf
