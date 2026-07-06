#include "core/SettingsStore.h"

#include "core/Strings.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace nmf {
namespace {

std::string Narrow(const std::wstring& value) {
    return WideToUtf8(value);
}

std::wstring Widen(const std::string& value) {
    return Utf8ToWide(value);
}

nlohmann::json ToJson(const AppSettings& settings) {
    nlohmann::json extensions = nlohmann::json::array();
    for (const auto& extension : settings.markdownView.extensions) {
        extensions.push_back(Narrow(extension));
    }

    return nlohmann::json{
        {"schemaVersion", settings.schemaVersion},
        {"markdownView", {
            {"defaultMode", Narrow(ToString(settings.markdownView.defaultMode))},
            {"toggleScope", Narrow(settings.markdownView.toggleScope)},
            {"refreshMode", Narrow(settings.markdownView.refreshMode)},
            {"extensions", extensions},
        }},
        {"outline", {
            {"visible", settings.outline.visible},
        }},
    };
}

AppSettings FromJson(const nlohmann::json& json) {
    AppSettings settings;
    settings.schemaVersion = json.value("schemaVersion", 1);

    const auto markdown = json.value("markdownView", nlohmann::json::object());
    settings.markdownView.defaultMode = DisplayModeFromString(Widen(markdown.value("defaultMode", "raw")));
    settings.markdownView.toggleScope = Widen(markdown.value("toggleScope", "global"));
    settings.markdownView.refreshMode = Widen(markdown.value("refreshMode", "onSave"));

    settings.markdownView.extensions.clear();
    if (markdown.contains("extensions") && markdown["extensions"].is_array()) {
        for (const auto& item : markdown["extensions"]) {
            if (item.is_string()) {
                auto ext = Trim(Widen(item.get<std::string>()));
                if (!ext.empty()) {
                    if (ext.front() != L'.') {
                        ext.insert(ext.begin(), L'.');
                    }
                    settings.markdownView.extensions.push_back(ToLower(ext));
                }
            }
        }
    }
    if (settings.markdownView.extensions.empty()) {
        settings.markdownView.extensions = {L".md", L".markdown"};
    }

    const auto outline = json.value("outline", nlohmann::json::object());
    settings.outline.visible = outline.value("visible", false);
    return settings;
}

}  // namespace

SettingsStore::SettingsStore(std::filesystem::path settingsPath) : settingsPath_(std::move(settingsPath)) {}

const std::filesystem::path& SettingsStore::Path() const {
    return settingsPath_;
}

AppSettings SettingsStore::Load() const {
    if (!std::filesystem::exists(settingsPath_)) {
        return {};
    }

    std::ifstream input(settingsPath_, std::ios::binary);
    if (!input) {
        return {};
    }

    try {
        nlohmann::json json;
        input >> json;
        return FromJson(json);
    } catch (...) {
        return {};
    }
}

void SettingsStore::Save(const AppSettings& settings) const {
    std::filesystem::create_directories(settingsPath_.parent_path());
    std::ofstream output(settingsPath_, std::ios::binary | std::ios::trunc);
    output << ToJson(settings).dump(2);
    output << '\n';
}

}  // namespace nmf
