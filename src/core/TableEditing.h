#pragma once

#include <optional>
#include <string>
#include <vector>

namespace nmf {

enum class TableAlignment {
    None,
    Left,
    Center,
    Right,
};

struct MarkdownTable {
    std::string indent;                          // leading whitespace of the header line
    std::vector<std::string> header;             // trimmed cell contents
    std::vector<TableAlignment> alignments;      // one per column
    std::vector<std::vector<std::string>> rows;  // trimmed cell contents per body row
};

struct TableSpan {
    int firstLine{0};  // header line index within the queried lines
    int lastLine{0};   // last body row line index (inclusive)
    bool hasDelimiter{false};
};

// True for lines that can belong to a pipe table (contain an unescaped '|').
bool IsTableLine(const std::string& line);

// Locate the contiguous table block around caretLine. A single pipe line
// without a delimiter row is reported with hasDelimiter=false (scaffold case).
std::optional<TableSpan> FindTableBlock(const std::vector<std::string>& lines, int caretLine);

// Split one table line into trimmed cells, honoring escaped pipes ("\|").
std::vector<std::string> SplitTableRow(const std::string& line);

std::optional<MarkdownTable> ParseTable(const std::vector<std::string>& tableLines);

// Render the table as aligned, pipe-delimited lines. Column widths use
// UTF-8 codepoint counts (East Asian double-width is not accounted for).
std::vector<std::string> FormatTable(const MarkdownTable& table);

// Column operations; index is clamped to the valid range. Rows shorter than
// the header are padded first so every row is affected consistently.
void InsertTableColumn(MarkdownTable& table, size_t afterIndex);
bool DeleteTableColumn(MarkdownTable& table, size_t index);  // false if last column
void CycleTableColumnAlignment(MarkdownTable& table, size_t index);

// Byte offset ranges of each cell's content in a FormatTable output line.
struct CellRange {
    size_t start{0};
    size_t end{0};
};
std::vector<CellRange> FormattedCellRanges(const std::string& formattedLine);

// Which cell (0-based) does byte column `byteColumn` fall into on this line?
size_t CellIndexForColumn(const std::string& line, size_t byteColumn);

size_t Utf8CodepointCount(const std::string& value);

}  // namespace nmf
