#include "core/DocumentStats.h"

#include <algorithm>
#include <cctype>

namespace nmf {
namespace {

std::string TrimRight(const std::string& value) {
    auto end = value.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(0, end);
}

bool IsFenceLine(const std::string& line) {
    size_t index = 0;
    while (index < line.size() && (line[index] == ' ' || line[index] == '\t')) {
        ++index;
    }
    return line.compare(index, 3, "```") == 0 || line.compare(index, 3, "~~~") == 0;
}

bool IsWordByte(unsigned char ch) {
    return std::isalnum(ch) != 0 || ch >= 0x80;
}

}  // namespace

std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (size_t index = 0; index < text.size(); ++index) {
        const char ch = text[index];
        if (ch == '\r') {
            lines.push_back(current);
            current.clear();
            if (index + 1 < text.size() && text[index + 1] == '\n') {
                ++index;
            }
        } else if (ch == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    lines.push_back(current);
    return lines;
}

FrontmatterInfo DetectFrontmatter(const std::vector<std::string>& lines) {
    FrontmatterInfo info;
    if (lines.empty() || TrimRight(lines[0]) != "---") {
        return info;
    }
    for (int index = 1; index < static_cast<int>(lines.size()); ++index) {
        const auto trimmed = TrimRight(lines[static_cast<size_t>(index)]);
        if (trimmed == "---" || trimmed == "...") {
            info.present = true;
            info.endLine = index;
            return info;
        }
    }
    return info;
}

DocumentStatistics ComputeStats(const std::vector<std::string>& lines) {
    DocumentStatistics stats;
    const auto frontmatter = DetectFrontmatter(lines);
    bool inFence = false;
    for (int index = 0; index < static_cast<int>(lines.size()); ++index) {
        if (frontmatter.present && index <= frontmatter.endLine) {
            continue;
        }
        const auto& line = lines[static_cast<size_t>(index)];
        if (IsFenceLine(line)) {
            inFence = !inFence;
            continue;
        }
        if (inFence) {
            continue;
        }
        bool inWord = false;
        for (const char raw : line) {
            const auto ch = static_cast<unsigned char>(raw);
            // Codepoint counting: skip UTF-8 continuation bytes.
            if ((ch & 0xC0) != 0x80) {
                ++stats.characters;
            }
            if (IsWordByte(ch)) {
                if (!inWord) {
                    ++stats.words;
                    inWord = true;
                }
            } else {
                inWord = false;
            }
        }
    }
    if (stats.words > 0) {
        stats.readingMinutes = std::max(1, (stats.words + 199) / 200);
    }
    return stats;
}

std::vector<std::string> BreadcrumbChain(const std::vector<MarkdownHeading>& headings, int caretLine) {
    std::vector<MarkdownHeading> stack;
    for (const auto& heading : headings) {
        if (heading.line > caretLine) {
            break;
        }
        while (!stack.empty() && stack.back().level >= heading.level) {
            stack.pop_back();
        }
        stack.push_back(heading);
    }
    std::vector<std::string> chain;
    chain.reserve(stack.size());
    for (const auto& heading : stack) {
        chain.push_back(heading.text);
    }
    return chain;
}

}  // namespace nmf
