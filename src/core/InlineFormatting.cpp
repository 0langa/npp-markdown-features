#include "core/InlineFormatting.h"

#include <algorithm>
#include <cctype>

namespace nmf {
namespace {

bool EndsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool StartsWith(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool IsBlank(const std::string& line) {
    return std::all_of(line.begin(), line.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
}

}  // namespace

bool ShouldUnwrapMarker(const std::string& before, const std::string& after, const std::string& marker) {
    if (!EndsWith(before, marker) || !StartsWith(after, marker)) {
        return false;
    }
    if (marker == "*") {
        // Don't mistake the inner edge of a bold span for italic markers.
        const bool boldBefore = EndsWith(before, "**");
        const bool boldAfter = StartsWith(after, "**");
        if (boldBefore && boldAfter) {
            return false;
        }
    }
    return true;
}

std::string ChangeHeadingLevel(const std::string& line, int delta) {
    size_t indentEnd = 0;
    while (indentEnd < line.size() && (line[indentEnd] == ' ' || line[indentEnd] == '\t')) {
        ++indentEnd;
    }
    size_t hashEnd = indentEnd;
    while (hashEnd < line.size() && line[hashEnd] == '#') {
        ++hashEnd;
    }
    int level = static_cast<int>(hashEnd - indentEnd);
    std::string rest;
    if (level > 0 && level <= 6 && (hashEnd == line.size() || line[hashEnd] == ' ' || line[hashEnd] == '\t')) {
        size_t contentStart = hashEnd;
        while (contentStart < line.size() && (line[contentStart] == ' ' || line[contentStart] == '\t')) {
            ++contentStart;
        }
        rest = line.substr(contentStart);
    } else {
        level = 0;
        rest = line.substr(indentEnd);
    }

    const int newLevel = std::clamp(level + delta, 0, 6);
    if (newLevel == 0) {
        return line.substr(0, indentEnd) + rest;
    }
    return line.substr(0, indentEnd) + std::string(static_cast<size_t>(newLevel), '#') + " " + rest;
}

std::vector<std::string> ToggleBlockquoteLines(const std::vector<std::string>& lines) {
    bool allQuoted = true;
    bool anyContent = false;
    for (const auto& line : lines) {
        if (IsBlank(line)) {
            continue;
        }
        anyContent = true;
        size_t index = 0;
        while (index < line.size() && (line[index] == ' ' || line[index] == '\t')) {
            ++index;
        }
        if (index >= line.size() || line[index] != '>') {
            allQuoted = false;
            break;
        }
    }

    std::vector<std::string> result;
    result.reserve(lines.size());
    if (anyContent && allQuoted) {
        // Remove one quote level.
        for (const auto& line : lines) {
            size_t index = 0;
            while (index < line.size() && (line[index] == ' ' || line[index] == '\t')) {
                ++index;
            }
            if (index < line.size() && line[index] == '>') {
                size_t after = index + 1;
                if (after < line.size() && line[after] == ' ') {
                    ++after;
                }
                result.push_back(line.substr(0, index) + line.substr(after));
            } else {
                result.push_back(line);
            }
        }
    } else {
        for (const auto& line : lines) {
            result.push_back(IsBlank(line) ? ">" : "> " + line);
        }
    }
    return result;
}

}  // namespace nmf
