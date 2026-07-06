#pragma once

#include "core/MarkdownOutline.h"

#include <string>
#include <vector>

namespace nmf {

struct FrontmatterInfo {
    bool present{false};
    int endLine{-1};  // 0-based line of the closing --- / ...
};

// YAML frontmatter: first line exactly "---", closed by "---" or "...".
FrontmatterInfo DetectFrontmatter(const std::vector<std::string>& lines);

struct DocumentStatistics {
    int words{0};
    int characters{0};      // UTF-8 codepoints, EOLs excluded
    int readingMinutes{0};  // ~200 words per minute, >=1 when words > 0
};

// Counts exclude YAML frontmatter and fenced code blocks.
DocumentStatistics ComputeStats(const std::vector<std::string>& lines);

// Heading chain containing `caretLine`, outermost first ("One › Two").
std::vector<std::string> BreadcrumbChain(const std::vector<MarkdownHeading>& headings, int caretLine);

// Splits UTF-8 text into lines (handles \r\n, \n, \r).
std::vector<std::string> SplitLines(const std::string& text);

}  // namespace nmf
