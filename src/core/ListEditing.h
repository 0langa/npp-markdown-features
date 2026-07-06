#pragma once

#include <optional>
#include <string>
#include <vector>

namespace nmf {

// Parsed shape of a Markdown list/quote line. All fields describe the line's
// prefix; content is everything after marker and following spaces.
struct ListItemInfo {
    std::string indent;           // leading whitespace, verbatim
    bool isQuote{false};          // ">"-prefixed line
    std::string quotePrefix;      // e.g. "> " or "> > ", verbatim, for quotes
    bool isOrdered{false};
    int orderedNumber{0};
    int orderedWidth{0};          // digit count incl. leading zeros, e.g. "01." -> 2
    char orderedDelimiter{'.'};   // '.' or ')'
    char bulletChar{'-'};         // for unordered items
    bool isTask{false};
    bool taskChecked{false};
    std::string content;          // text after the marker (trimmed of the single following space run)
};

std::optional<ListItemInfo> ParseListItem(const std::string& line);

// What smart-Enter should do after `previousLine`.
struct ContinuationResult {
    std::string prefix;   // text to start the new line with (empty when terminating)
    bool terminate{false};// previous line was an empty item: remove it, end the list
};

std::optional<ContinuationResult> ContinuationForLine(const std::string& previousLine);

// Toggle or add a task checkbox on one line (see command docs in README).
std::string ToggleTaskCheckbox(const std::string& line);

// Renumber ordered items in a block of lines. Counters are tracked per
// indent depth; the first item of each depth keeps its starting number.
std::vector<std::string> RenumberListBlock(const std::vector<std::string>& lines);

// True when the line belongs to a list block for renumbering purposes:
// a list item, an indented continuation, or a quote line.
bool IsListBlockLine(const std::string& line);

}  // namespace nmf
