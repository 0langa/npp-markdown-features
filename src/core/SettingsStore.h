#pragma once

#include "core/Settings.h"

#include <filesystem>

namespace nmf {

class SettingsStore {
public:
    explicit SettingsStore(std::filesystem::path settingsPath);

    const std::filesystem::path& Path() const;
    AppSettings Load() const;
    void Save(const AppSettings& settings) const;

private:
    std::filesystem::path settingsPath_;
};

}  // namespace nmf
