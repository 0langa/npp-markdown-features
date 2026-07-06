#include "core/TocGenerator.h"

#include "core/LinkTools.h"
#include "core/ListEditing.h"
#include "core/MarkdownOutline.h"
#include "core/TableEditing.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace nmf {
namespace {

std::string TrimAscii(const std::string& value) {
    size_t begin = 0;
    size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string ToLowerAscii(std::string value) {
    for (auto& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

struct MarkerRange {
    int start{-1};  // line of <!-- toc -->
    int end{-1};    // line of <!-- /toc -->
    bool Found() const {
        return start >= 0 && end > start;
    }
};

MarkerRange FindTocMarkers(const std::vector<std::string>& lines) {
    MarkerRange range;
    for (int index = 0; index < static_cast<int>(lines.size()); ++index) {
        const auto trimmed = ToLowerAscii(TrimAscii(lines[static_cast<size_t>(index)]));
        if (range.start < 0 && trimmed == kTocStartMarker) {
            range.start = index;
        } else if (range.start >= 0 && trimmed == kTocEndMarker) {
            range.end = index;
            break;
        }
    }
    return range;
}

bool IsFenceLine(const std::string& line) {
    const auto trimmed = TrimAscii(line);
    return trimmed.rfind("```", 0) == 0 || trimmed.rfind("~~~", 0) == 0;
}

bool IsAtxHeading(const std::string& line) {
    const auto trimmed = TrimAscii(line);
    if (trimmed.empty() || trimmed[0] != '#') {
        return false;
    }
    size_t hashes = 0;
    while (hashes < trimmed.size() && trimmed[hashes] == '#') {
        ++hashes;
    }
    return hashes <= 6 && (hashes == trimmed.size() || trimmed[hashes] == ' ' || trimmed[hashes] == '\t');
}

bool IsBlank(const std::string& line) {
    return TrimAscii(line).empty();
}

}  // namespace

std::vector<std::string> BuildTocLines(const std::vector<std::string>& documentLines) {
    const auto markers = FindTocMarkers(documentLines);
    std::string joined;
    for (const auto& line : documentLines) {
        joined += line;
        joined += '\n';
    }
    const auto outline = MarkdownOutline::Parse(joined);

    // Collect included headings.
    std::vector<MarkdownHeading> headings;
    int h1Count = 0;
    for (const auto& heading : outline.Headings()) {
        if (heading.level == 1) {
            ++h1Count;
        }
    }
    bool skippedTitle = false;
    for (const auto& heading : outline.Headings()) {
        if (markers.Found() && heading.line >= markers.start && heading.line <= markers.end) {
            continue;
        }
        if (!skippedTitle && heading.level == 1 && h1Count == 1 && headings.empty()) {
            skippedTitle = true;  // single H1 = document title
            continue;
        }
        headings.push_back(heading);
    }
    if (headings.empty()) {
        return {};
    }

    const int minLevel = std::min_element(headings.begin(), headings.end(), [](const auto& a, const auto& b) { return a.level < b.level; })->level;

    std::map<std::string, int> slugCounts;
    std::vector<std::string> toc;
    toc.reserve(headings.size());
    for (const auto& heading : headings) {
        auto slug = GithubSlug(heading.text);
        if (slug.empty()) {
            slug = "section";
        }
        const int duplicate = slugCounts[slug]++;
        if (duplicate > 0) {
            slug += "-" + std::to_string(duplicate);  // GitHub duplicate style
        }
        std::string line(static_cast<size_t>(heading.level - minLevel) * 2, ' ');
        line += "- [" + heading.text + "](#" + slug + ")";
        toc.push_back(std::move(line));
    }
    return toc;
}

std::vector<std::string> UpdateToc(const std::vector<std::string>& documentLines, int caretLine, bool& changed) {
    const auto toc = BuildTocLines(documentLines);
    const auto markers = FindTocMarkers(documentLines);

    std::vector<std::string> result;
    result.reserve(documentLines.size() + toc.size() + 2);
    if (markers.Found()) {
        for (int index = 0; index < static_cast<int>(documentLines.size()); ++index) {
            if (index == markers.start) {
                result.push_back(documentLines[static_cast<size_t>(index)]);
                result.insert(result.end(), toc.begin(), toc.end());
            } else if (index > markers.start && index < markers.end) {
                continue;  // old TOC content
            } else {
                result.push_back(documentLines[static_cast<size_t>(index)]);
            }
        }
    } else {
        const int insertAt = std::clamp(caretLine, 0, static_cast<int>(documentLines.size()));
        result.assign(documentLines.begin(), documentLines.begin() + insertAt);
        result.push_back(kTocStartMarker);
        result.insert(result.end(), toc.begin(), toc.end());
        result.push_back(kTocEndMarker);
        result.insert(result.end(), documentLines.begin() + insertAt, documentLines.end());
    }
    changed = result != documentLines;
    return result;
}

std::vector<std::string> CleanupDocumentLines(std::vector<std::string> lines, const CleanupOptions& options) {
    // Pass 1: line-local fixes, fence-aware.
    bool inFence = false;
    for (auto& line : lines) {
        const bool fenceLine = IsFenceLine(line);
        if (fenceLine) {
            inFence = !inFence;
            continue;
        }
        if (inFence) {
            continue;
        }
        if (options.trimTrailingWhitespace) {
            const auto contentEnd = line.find_last_not_of(" \t");
            if (contentEnd == std::string::npos) {
                line.clear();
            } else {
                const size_t trailingSpaces = line.size() - contentEnd - 1;
                // Two or more trailing spaces form a hard break; keep exactly two.
                const bool hardBreak = trailingSpaces >= 2 && line.find('\t', contentEnd + 1) == std::string::npos;
                line.resize(contentEnd + 1);
                if (hardBreak) {
                    line += "  ";
                }
            }
        }
        if (options.normalizeBulletMarkers) {
            if (const auto item = ParseListItem(line)) {
                if (!item->isQuote && !item->isOrdered && item->indent.size() <= 3
                    && (item->bulletChar == '*' || item->bulletChar == '+')) {
                    const auto markerPos = item->indent.size();
                    if (markerPos < line.size()) {
                        line[markerPos] = '-';
                    }
                }
            }
        }
    }

    // Pass 2: reformat tables (outside fences).
    if (options.formatTables) {
        inFence = false;
        for (size_t index = 0; index < lines.size();) {
            if (IsFenceLine(lines[index])) {
                inFence = !inFence;
                ++index;
                continue;
            }
            if (!inFence && index + 1 < lines.size() && IsTableLine(lines[index])) {
                const auto span = FindTableBlock(lines, static_cast<int>(index));
                if (span && span->hasDelimiter) {
                    const std::vector<std::string> block(lines.begin() + span->firstLine, lines.begin() + span->lastLine + 1);
                    if (const auto table = ParseTable(block)) {
                        const auto formatted = FormatTable(*table);
                        if (formatted.size() == block.size()) {
                            std::copy(formatted.begin(), formatted.end(), lines.begin() + span->firstLine);
                        }
                        index = static_cast<size_t>(span->lastLine) + 1;
                        continue;
                    }
                }
            }
            ++index;
        }
    }

    // Pass 3: blank-line management.
    std::vector<std::string> result;
    result.reserve(lines.size());
    inFence = false;
    int blankRun = 0;
    for (size_t index = 0; index < lines.size(); ++index) {
        const auto& line = lines[index];
        const bool fenceLine = IsFenceLine(line);
        const bool blank = !inFence && !fenceLine && IsBlank(line);

        if (blank) {
            ++blankRun;
            if (options.collapseBlankLines && blankRun > 1) {
                continue;
            }
            result.push_back("");
            continue;
        }

        const bool needsBlankBefore = !inFence
            && ((options.blankAroundHeadings && IsAtxHeading(line))
                || (options.blankAroundFences && fenceLine && !inFence));
        if (needsBlankBefore && !result.empty() && !result.back().empty()) {
            result.push_back("");
        }
        result.push_back(line);
        blankRun = 0;

        const bool closesFence = fenceLine && inFence;
        if (fenceLine) {
            inFence = !inFence;
        }
        // Blank line after a heading or a closing fence (unless one follows).
        const bool needsBlankAfter = !inFence
            && ((options.blankAroundHeadings && IsAtxHeading(line))
                || (options.blankAroundFences && closesFence));
        if (needsBlankAfter && index + 1 < lines.size() && !IsBlank(lines[index + 1])) {
            result.push_back("");
            blankRun = 1;
        }
    }

    if (options.singleFinalNewline) {
        while (!result.empty() && result.back().empty()) {
            result.pop_back();
        }
    }
    return result;
}

}  // namespace nmf
