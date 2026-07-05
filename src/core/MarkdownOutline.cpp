#include "core/MarkdownOutline.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>

namespace nmf {
namespace {

std::string TrimAscii(std::string value) {
    const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

bool IsAtxHeading(const std::string& line, int& level, std::string& text) {
    size_t index = 0;
    while (index < line.size() && line[index] == '#') {
        ++index;
    }
    if (index == 0 || index > 6 || index >= line.size() || std::isspace(static_cast<unsigned char>(line[index])) == 0) {
        return false;
    }
    level = static_cast<int>(index);
    text = TrimAscii(line.substr(index));
    while (!text.empty() && text.back() == '#') {
        text.pop_back();
    }
    text = TrimAscii(text);
    return !text.empty();
}

}  // namespace

MarkdownOutline MarkdownOutline::Parse(const std::string& markdownUtf8) {
    MarkdownOutline outline;
    std::map<std::string, int> anchorCounts;
    std::istringstream stream(markdownUtf8);
    std::string line;
    int lineNumber = 0;
    bool fencedCode = false;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto trimmed = TrimAscii(line);
        if (trimmed.rfind("```", 0) == 0 || trimmed.rfind("~~~", 0) == 0) {
            fencedCode = !fencedCode;
            ++lineNumber;
            continue;
        }
        if (!fencedCode) {
            int level = 0;
            std::string text;
            if (IsAtxHeading(trimmed, level, text)) {
                const auto base = SlugifyHeading(text, 0);
                const int duplicate = anchorCounts[base]++;
                outline.headings_.push_back({lineNumber, level, text, SlugifyHeading(text, duplicate)});
            }
        }
        ++lineNumber;
    }

    return outline;
}

const std::vector<MarkdownHeading>& MarkdownOutline::Headings() const {
    return headings_;
}

std::optional<MarkdownHeading> MarkdownOutline::HeadingAtOrBeforeLine(int line) const {
    std::optional<MarkdownHeading> result;
    for (const auto& heading : headings_) {
        if (heading.line > line) {
            break;
        }
        result = heading;
    }
    return result;
}

std::optional<MarkdownHeading> MarkdownOutline::HeadingByAnchor(const std::string& anchorId) const {
    const auto it = std::find_if(headings_.begin(), headings_.end(), [&](const MarkdownHeading& heading) { return heading.anchorId == anchorId; });
    if (it == headings_.end()) {
        return std::nullopt;
    }
    return *it;
}

std::string SlugifyHeading(const std::string& headingText, int duplicateIndex) {
    std::string slug;
    bool previousDash = false;
    for (unsigned char ch : headingText) {
        if (std::isalnum(ch) != 0) {
            slug.push_back(static_cast<char>(std::tolower(ch)));
            previousDash = false;
        } else if ((std::isspace(ch) != 0 || ch == '-' || ch == '_') && !slug.empty() && !previousDash) {
            slug.push_back('-');
            previousDash = true;
        }
    }
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    if (slug.empty()) {
        slug = "section";
    }
    if (duplicateIndex > 0) {
        slug += "-" + std::to_string(duplicateIndex + 1);
    }
    return "nmf-heading-" + slug;
}

}  // namespace nmf
