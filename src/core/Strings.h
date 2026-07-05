#pragma once

#include <string>
#include <vector>

namespace nmf {

std::wstring ToLower(std::wstring value);
bool EndsWithMarkdownExtension(const std::wstring& path, const std::vector<std::wstring>& extensions);
std::string WideToUtf8(const std::wstring& value);
std::wstring Utf8ToWide(const std::string& value);
std::string WideToAnsiBytes(const std::wstring& value);
std::wstring Trim(const std::wstring& value);
std::vector<std::wstring> SplitExtensions(const std::wstring& value);
std::wstring JoinExtensions(const std::vector<std::wstring>& extensions);

}  // namespace nmf
