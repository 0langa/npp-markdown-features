#include "rendering/WebViewUserDataFolder.h"

#include <array>
#include <windows.h>

namespace nmf {
namespace {

std::filesystem::path KnownWritableRoot() {
    std::array<wchar_t, 32768> buffer{};
    DWORD length = ::GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length > 0 && length < buffer.size()) {
        return std::filesystem::path(buffer.data());
    }

    length = ::GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (length > 0 && length < buffer.size()) {
        return std::filesystem::path(buffer.data());
    }

    return std::filesystem::temp_directory_path();
}

}  // namespace

std::filesystem::path DefaultWebViewUserDataFolder() {
    return KnownWritableRoot() / L"NppMarkdownFeatures" / L"WebView2";
}

}  // namespace nmf
