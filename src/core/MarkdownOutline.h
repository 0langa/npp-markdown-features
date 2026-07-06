#pragma once

#include <optional>
#include <string>
#include <vector>

namespace nmf {

struct MarkdownHeading {
    int line{0};
    int level{1};
    std::string text;
    std::string anchorId;
};

inline bool operator==(const MarkdownHeading& a, const MarkdownHeading& b) {
    return a.line == b.line && a.level == b.level && a.text == b.text && a.anchorId == b.anchorId;
}

class MarkdownOutline {
public:
    static MarkdownOutline Parse(const std::string& markdownUtf8);

    const std::vector<MarkdownHeading>& Headings() const;
    std::optional<MarkdownHeading> HeadingAtOrBeforeLine(int line) const;
    std::optional<MarkdownHeading> HeadingByAnchor(const std::string& anchorId) const;

private:
    std::vector<MarkdownHeading> headings_;
};

std::string SlugifyHeading(const std::string& headingText, int duplicateIndex);

}  // namespace nmf
