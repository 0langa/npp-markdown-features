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

struct AppSettings {
    int schemaVersion{1};
    MarkdownViewSettings markdownView{};
};

std::wstring ToString(DisplayMode mode);
DisplayMode DisplayModeFromString(const std::wstring& value);

}  // namespace nmf
