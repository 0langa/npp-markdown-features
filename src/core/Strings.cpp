#include "core/Strings.h"

#include <algorithm>
#include <cwctype>
#include <sstream>
#include <stdexcept>
#include <windows.h>

namespace nmf {

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

bool EndsWithMarkdownExtension(const std::wstring& path, const std::vector<std::wstring>& extensions) {
    const auto lowerPath = ToLower(path);
    for (auto ext : extensions) {
        ext = ToLower(Trim(ext));
        if (ext.empty()) {
            continue;
        }
        if (ext.front() != L'.') {
            ext.insert(ext.begin(), L'.');
        }
        if (lowerPath.size() >= ext.size() && lowerPath.compare(lowerPath.size() - ext.size(), ext.size(), ext) == 0) {
            return true;
        }
    }
    return false;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        throw std::runtime_error("WideCharToMultiByte(CP_UTF8) failed");
    }
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        throw std::runtime_error("MultiByteToWideChar(CP_UTF8) failed");
    }
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string WideToAnsiBytes(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_ACP, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        throw std::runtime_error("WideCharToMultiByte(CP_ACP) failed");
    }
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_ACP, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Trim(const std::wstring& value) {
    const auto begin = std::find_if_not(value.begin(), value.end(), [](wchar_t ch) { return std::iswspace(ch); });
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](wchar_t ch) { return std::iswspace(ch); }).base();
    if (begin >= end) {
        return {};
    }
    return std::wstring(begin, end);
}

std::vector<std::wstring> SplitExtensions(const std::wstring& value) {
    std::vector<std::wstring> extensions;
    std::wstring current;
    std::wstringstream stream(value);
    while (std::getline(stream, current, L',')) {
        current = Trim(current);
        if (!current.empty()) {
            if (current.front() != L'.') {
                current.insert(current.begin(), L'.');
            }
            extensions.push_back(ToLower(current));
        }
    }
    if (extensions.empty()) {
        extensions = {L".md", L".markdown"};
    }
    return extensions;
}

std::wstring JoinExtensions(const std::vector<std::wstring>& extensions) {
    std::wstring result;
    for (size_t i = 0; i < extensions.size(); ++i) {
        if (i > 0) {
            result += L", ";
        }
        result += extensions[i];
    }
    return result;
}

}  // namespace nmf
