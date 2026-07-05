#pragma once

#include "core/Settings.h"

#include <filesystem>
#include <windows.h>

namespace nmf {

bool ShowSettingsDialog(HWND owner, MarkdownViewSettings& settings, const std::filesystem::path& settingsPath);

}  // namespace nmf
