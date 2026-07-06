#pragma once

#include <string>
#include <vector>

namespace nmf {

// Marker lines delimiting a generated TOC block.
inline constexpr char kTocStartMarker[] = "<!-- toc -->";
inline constexpr char kTocEndMarker[] = "<!-- /toc -->";

// Build the TOC bullet lines for a document (without markers). Headings
// between existing TOC markers are excluded; a single leading H1 is treated
// as the document title and skipped. Slugs follow GitHub rules with -1/-2
// duplicate suffixes.
std::vector<std::string> BuildTocLines(const std::vector<std::string>& documentLines);

// Insert (at caretLine) or update (between existing markers) the TOC block.
// Returns the new document lines; `changed` reports whether anything differs.
std::vector<std::string> UpdateToc(const std::vector<std::string>& documentLines, int caretLine, bool& changed);

// Document cleanup: conservative Markdown normalization.
struct CleanupOptions {
    bool trimTrailingWhitespace{true};   // keeps exactly-two-space hard breaks
    bool collapseBlankLines{true};       // max one consecutive blank line
    bool blankAroundHeadings{true};
    bool blankAroundFences{true};
    bool normalizeBulletMarkers{true};   // * and + become - (indent <= 3 only)
    bool formatTables{true};
    bool singleFinalNewline{true};       // callers join with EOL; empty last line
};

std::vector<std::string> CleanupDocumentLines(std::vector<std::string> lines, const CleanupOptions& options);

}  // namespace nmf
