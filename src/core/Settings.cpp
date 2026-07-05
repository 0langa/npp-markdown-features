#include "core/Settings.h"

#include "core/Strings.h"

namespace nmf {

std::wstring ToString(DisplayMode mode) {
    return mode == DisplayMode::Rendered ? L"rendered" : L"raw";
}

DisplayMode DisplayModeFromString(const std::wstring& value) {
    return ToLower(value) == L"rendered" ? DisplayMode::Rendered : DisplayMode::Raw;
}

}  // namespace nmf
