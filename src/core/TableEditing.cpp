#include "core/TableEditing.h"

#include <algorithm>
#include <cctype>

namespace nmf {
namespace {

std::string TrimSpaces(const std::string& value) {
    size_t begin = 0;
    size_t end = value.size();
    while (begin < end && (value[begin] == ' ' || value[begin] == '\t')) {
        ++begin;
    }
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t')) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string IndentOf(const std::string& line) {
    size_t index = 0;
    while (index < line.size() && (line[index] == ' ' || line[index] == '\t')) {
        ++index;
    }
    return line.substr(0, index);
}

bool IsDelimiterCell(const std::string& cell) {
    const auto trimmed = TrimSpaces(cell);
    if (trimmed.empty()) {
        return false;
    }
    size_t index = 0;
    if (trimmed[index] == ':') {
        ++index;
    }
    size_t dashes = 0;
    while (index < trimmed.size() && trimmed[index] == '-') {
        ++index;
        ++dashes;
    }
    if (index < trimmed.size() && trimmed[index] == ':') {
        ++index;
    }
    return dashes >= 1 && index == trimmed.size();
}

bool IsDelimiterRow(const std::string& line) {
    if (!IsTableLine(line)) {
        return false;
    }
    const auto cells = SplitTableRow(line);
    if (cells.empty()) {
        return false;
    }
    return std::all_of(cells.begin(), cells.end(), [](const std::string& cell) { return IsDelimiterCell(cell); });
}

TableAlignment AlignmentFromDelimiter(const std::string& cell) {
    const auto trimmed = TrimSpaces(cell);
    const bool left = !trimmed.empty() && trimmed.front() == ':';
    const bool right = !trimmed.empty() && trimmed.back() == ':';
    if (left && right) {
        return TableAlignment::Center;
    }
    if (right) {
        return TableAlignment::Right;
    }
    if (left) {
        return TableAlignment::Left;
    }
    return TableAlignment::None;
}

std::string DelimiterForAlignment(TableAlignment alignment, size_t width) {
    width = std::max<size_t>(width, 3);
    switch (alignment) {
        case TableAlignment::Left:
            return ":" + std::string(width - 1, '-');
        case TableAlignment::Right:
            return std::string(width - 1, '-') + ":";
        case TableAlignment::Center:
            return ":" + std::string(width - 2, '-') + ":";
        case TableAlignment::None:
        default:
            return std::string(width, '-');
    }
}

std::string PadCell(const std::string& content, size_t width, TableAlignment alignment) {
    const size_t length = Utf8CodepointCount(content);
    if (length >= width) {
        return content;
    }
    const size_t total = width - length;
    if (alignment == TableAlignment::Center) {
        const size_t left = total / 2;
        return std::string(left, ' ') + content + std::string(total - left, ' ');
    }
    if (alignment == TableAlignment::Right) {
        return std::string(total, ' ') + content;
    }
    return content + std::string(total, ' ');
}

}  // namespace

size_t Utf8CodepointCount(const std::string& value) {
    size_t count = 0;
    for (const char ch : value) {
        if ((static_cast<unsigned char>(ch) & 0xC0) != 0x80) {
            ++count;
        }
    }
    return count;
}

bool IsTableLine(const std::string& line) {
    const auto trimmed = TrimSpaces(line);
    if (trimmed.empty()) {
        return false;
    }
    for (size_t index = 0; index < trimmed.size(); ++index) {
        if (trimmed[index] == '|' && (index == 0 || trimmed[index - 1] != '\\')) {
            return true;
        }
    }
    return false;
}

std::optional<TableSpan> FindTableBlock(const std::vector<std::string>& lines, int caretLine) {
    if (caretLine < 0 || caretLine >= static_cast<int>(lines.size()) || !IsTableLine(lines[static_cast<size_t>(caretLine)])) {
        return std::nullopt;
    }
    int first = caretLine;
    while (first > 0 && IsTableLine(lines[static_cast<size_t>(first - 1)])) {
        --first;
    }
    int last = caretLine;
    while (last + 1 < static_cast<int>(lines.size()) && IsTableLine(lines[static_cast<size_t>(last + 1)])) {
        ++last;
    }
    TableSpan span{first, last, false};
    if (last > first && IsDelimiterRow(lines[static_cast<size_t>(first + 1)])) {
        span.hasDelimiter = true;
    }
    return span;
}

std::vector<std::string> SplitTableRow(const std::string& line) {
    const auto trimmed = TrimSpaces(line);
    std::vector<std::string> cells;
    std::string current;
    size_t index = 0;
    // Skip a single leading pipe.
    if (!trimmed.empty() && trimmed.front() == '|') {
        index = 1;
    }
    for (; index < trimmed.size(); ++index) {
        const char ch = trimmed[index];
        if (ch == '\\' && index + 1 < trimmed.size() && trimmed[index + 1] == '|') {
            current += "\\|";
            ++index;
            continue;
        }
        if (ch == '|') {
            cells.push_back(TrimSpaces(current));
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    const auto tail = TrimSpaces(current);
    // A trailing pipe leaves an empty tail; only keep genuine content.
    if (!tail.empty() || (!trimmed.empty() && trimmed.back() != '|')) {
        cells.push_back(tail);
    }
    return cells;
}

std::optional<MarkdownTable> ParseTable(const std::vector<std::string>& tableLines) {
    if (tableLines.size() < 2 || !IsDelimiterRow(tableLines[1])) {
        return std::nullopt;
    }
    MarkdownTable table;
    table.indent = IndentOf(tableLines[0]);
    table.header = SplitTableRow(tableLines[0]);
    for (const auto& cell : SplitTableRow(tableLines[1])) {
        table.alignments.push_back(AlignmentFromDelimiter(cell));
    }
    const size_t columns = std::max(table.header.size(), table.alignments.size());
    table.header.resize(columns);
    table.alignments.resize(columns, TableAlignment::None);
    for (size_t index = 2; index < tableLines.size(); ++index) {
        auto cells = SplitTableRow(tableLines[index]);
        cells.resize(columns);
        table.rows.push_back(std::move(cells));
    }
    return table;
}

std::vector<std::string> FormatTable(const MarkdownTable& table) {
    const size_t columns = std::max<size_t>(table.header.size(), 1);
    std::vector<size_t> widths(columns, 3);
    for (size_t column = 0; column < columns; ++column) {
        if (column < table.header.size()) {
            widths[column] = std::max(widths[column], Utf8CodepointCount(table.header[column]));
        }
        for (const auto& row : table.rows) {
            if (column < row.size()) {
                widths[column] = std::max(widths[column], Utf8CodepointCount(row[column]));
            }
        }
    }

    const auto alignmentAt = [&](size_t column) {
        return column < table.alignments.size() ? table.alignments[column] : TableAlignment::None;
    };
    const auto renderRow = [&](const std::vector<std::string>& cells) {
        std::string line = table.indent + "|";
        for (size_t column = 0; column < columns; ++column) {
            const std::string content = column < cells.size() ? cells[column] : std::string{};
            line += " " + PadCell(content, widths[column], alignmentAt(column)) + " |";
        }
        return line;
    };

    std::vector<std::string> lines;
    lines.push_back(renderRow(table.header));
    std::string delimiter = table.indent + "|";
    for (size_t column = 0; column < columns; ++column) {
        delimiter += " " + DelimiterForAlignment(alignmentAt(column), widths[column]) + " |";
    }
    lines.push_back(delimiter);
    for (const auto& row : table.rows) {
        lines.push_back(renderRow(row));
    }
    return lines;
}

void InsertTableColumn(MarkdownTable& table, size_t afterIndex) {
    const size_t columns = table.header.size();
    const size_t position = columns == 0 ? 0 : std::min(afterIndex + 1, columns);
    table.header.insert(table.header.begin() + static_cast<ptrdiff_t>(position), "");
    table.alignments.resize(columns, TableAlignment::None);
    table.alignments.insert(table.alignments.begin() + static_cast<ptrdiff_t>(position), TableAlignment::None);
    for (auto& row : table.rows) {
        row.resize(columns);
        row.insert(row.begin() + static_cast<ptrdiff_t>(position), "");
    }
}

bool DeleteTableColumn(MarkdownTable& table, size_t index) {
    if (table.header.size() <= 1 || index >= table.header.size()) {
        return false;
    }
    table.header.erase(table.header.begin() + static_cast<ptrdiff_t>(index));
    if (index < table.alignments.size()) {
        table.alignments.erase(table.alignments.begin() + static_cast<ptrdiff_t>(index));
    }
    for (auto& row : table.rows) {
        if (index < row.size()) {
            row.erase(row.begin() + static_cast<ptrdiff_t>(index));
        }
    }
    return true;
}

void CycleTableColumnAlignment(MarkdownTable& table, size_t index) {
    if (index >= table.header.size()) {
        return;
    }
    if (table.alignments.size() < table.header.size()) {
        table.alignments.resize(table.header.size(), TableAlignment::None);
    }
    switch (table.alignments[index]) {
        case TableAlignment::None:
            table.alignments[index] = TableAlignment::Left;
            break;
        case TableAlignment::Left:
            table.alignments[index] = TableAlignment::Center;
            break;
        case TableAlignment::Center:
            table.alignments[index] = TableAlignment::Right;
            break;
        case TableAlignment::Right:
            table.alignments[index] = TableAlignment::None;
            break;
    }
}

std::vector<CellRange> FormattedCellRanges(const std::string& formattedLine) {
    std::vector<CellRange> ranges;
    std::vector<size_t> pipes;
    for (size_t index = 0; index < formattedLine.size(); ++index) {
        if (formattedLine[index] == '|' && (index == 0 || formattedLine[index - 1] != '\\')) {
            pipes.push_back(index);
        }
    }
    for (size_t pipe = 0; pipe + 1 < pipes.size(); ++pipe) {
        // Content sits between "| " and " |".
        size_t start = pipes[pipe] + 1;
        if (start < formattedLine.size() && formattedLine[start] == ' ') {
            ++start;
        }
        size_t end = pipes[pipe + 1];
        if (end > start && formattedLine[end - 1] == ' ') {
            --end;
        }
        ranges.push_back({start, std::max(start, end)});
    }
    return ranges;
}

size_t CellIndexForColumn(const std::string& line, size_t byteColumn) {
    size_t cell = 0;
    bool seenFirstPipe = false;
    const auto indentSize = IndentOf(line).size();
    for (size_t index = 0; index < std::min(byteColumn, line.size()); ++index) {
        if (line[index] == '|' && (index == 0 || line[index - 1] != '\\')) {
            if (!seenFirstPipe && index <= indentSize) {
                seenFirstPipe = true;  // leading pipe doesn't advance the cell
                continue;
            }
            ++cell;
        }
    }
    return cell;
}

}  // namespace nmf
