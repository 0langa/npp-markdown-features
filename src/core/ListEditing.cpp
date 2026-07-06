#include "core/ListEditing.h"

#include <algorithm>
#include <cctype>

namespace nmf {
namespace {

bool IsBlankOrSpaces(const std::string& value, size_t from = 0) {
    return std::all_of(value.begin() + static_cast<ptrdiff_t>(std::min(from, value.size())), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
}

size_t IndentLength(const std::string& line) {
    size_t index = 0;
    while (index < line.size() && (line[index] == ' ' || line[index] == '\t')) {
        ++index;
    }
    return index;
}

// Visual indent depth: tabs count as 4 columns for grouping purposes.
size_t IndentDepth(const std::string& indent) {
    size_t depth = 0;
    for (const char ch : indent) {
        depth += ch == '\t' ? 4 : 1;
    }
    return depth;
}

// Parses an optional task box at `index`; returns length consumed (0 if none).
size_t ParseTaskBox(const std::string& line, size_t index, bool& checked) {
    if (index + 2 >= line.size() || line[index] != '[' || line[index + 2] != ']') {
        return 0;
    }
    const char mark = line[index + 1];
    if (mark != ' ' && mark != 'x' && mark != 'X') {
        return 0;
    }
    // Must be followed by a space or end of line to be a task box.
    if (index + 3 < line.size() && line[index + 3] != ' ' && line[index + 3] != '\t') {
        return 0;
    }
    checked = mark != ' ';
    return 3;
}

}  // namespace

std::optional<ListItemInfo> ParseListItem(const std::string& line) {
    ListItemInfo info;
    const size_t indentLength = IndentLength(line);
    info.indent = line.substr(0, indentLength);
    size_t index = indentLength;
    if (index >= line.size()) {
        return std::nullopt;
    }

    if (line[index] == '>') {
        info.isQuote = true;
        size_t prefixEnd = index;
        while (prefixEnd < line.size() && (line[prefixEnd] == '>' || line[prefixEnd] == ' ')) {
            ++prefixEnd;
        }
        // Keep the exact quote prefix (supports nesting like "> > ").
        size_t lastMarker = line.find_last_of('>', prefixEnd - 1);
        size_t contentStart = lastMarker + 1;
        if (contentStart < line.size() && line[contentStart] == ' ') {
            ++contentStart;
        }
        info.quotePrefix = line.substr(indentLength, contentStart - indentLength);
        info.content = line.substr(contentStart);
        return info;
    }

    const char first = line[index];
    if (first == '-' || first == '*' || first == '+') {
        if (index + 1 < line.size() && line[index + 1] != ' ' && line[index + 1] != '\t') {
            return std::nullopt;
        }
        info.bulletChar = first;
        ++index;
    } else if (std::isdigit(static_cast<unsigned char>(first)) != 0) {
        size_t digitEnd = index;
        while (digitEnd < line.size() && std::isdigit(static_cast<unsigned char>(line[digitEnd])) != 0) {
            ++digitEnd;
        }
        if (digitEnd - index > 9 || digitEnd >= line.size() || (line[digitEnd] != '.' && line[digitEnd] != ')')) {
            return std::nullopt;
        }
        if (digitEnd + 1 < line.size() && line[digitEnd + 1] != ' ' && line[digitEnd + 1] != '\t') {
            return std::nullopt;
        }
        info.isOrdered = true;
        info.orderedNumber = std::stoi(line.substr(index, digitEnd - index));
        info.orderedWidth = static_cast<int>(digitEnd - index);
        info.orderedDelimiter = line[digitEnd];
        index = digitEnd + 1;
    } else {
        return std::nullopt;
    }

    // Skip the single run of spaces after the marker.
    while (index < line.size() && (line[index] == ' ' || line[index] == '\t')) {
        ++index;
    }
    bool checked = false;
    const size_t taskLength = ParseTaskBox(line, index, checked);
    if (taskLength > 0) {
        info.isTask = true;
        info.taskChecked = checked;
        index += taskLength;
        while (index < line.size() && (line[index] == ' ' || line[index] == '\t')) {
            ++index;
        }
    }
    info.content = line.substr(index);
    return info;
}

std::optional<ContinuationResult> ContinuationForLine(const std::string& previousLine) {
    const auto item = ParseListItem(previousLine);
    if (!item) {
        return std::nullopt;
    }

    if (item->isQuote) {
        if (IsBlankOrSpaces(item->content)) {
            return ContinuationResult{{}, true};
        }
        return ContinuationResult{item->indent + item->quotePrefix, false};
    }

    if (IsBlankOrSpaces(item->content)) {
        return ContinuationResult{{}, true};
    }

    std::string prefix = item->indent;
    if (item->isOrdered) {
        std::string number = std::to_string(item->orderedNumber + 1);
        // Preserve zero-padded widths like "01." -> "02.".
        while (static_cast<int>(number.size()) < item->orderedWidth) {
            number.insert(number.begin(), '0');
        }
        prefix += number;
        prefix.push_back(item->orderedDelimiter);
    } else {
        prefix.push_back(item->bulletChar);
    }
    prefix.push_back(' ');
    if (item->isTask) {
        prefix += "[ ] ";
    }
    return ContinuationResult{prefix, false};
}

std::string ToggleTaskCheckbox(const std::string& line) {
    const auto item = ParseListItem(line);
    if (item && !item->isQuote && item->isTask) {
        // Flip the existing box in place.
        const auto boxPos = line.find(item->taskChecked ? "[x]" : "[ ]");
        const auto boxPosUpper = line.find("[X]");
        auto position = boxPos;
        if (item->taskChecked && boxPosUpper != std::string::npos && (boxPos == std::string::npos || boxPosUpper < boxPos)) {
            position = boxPosUpper;
        }
        if (position == std::string::npos) {
            return line;
        }
        std::string toggled = line;
        toggled[position + 1] = item->taskChecked ? ' ' : 'x';
        return toggled;
    }
    if (item && !item->isQuote) {
        // Plain list item: insert an unchecked box after the marker.
        const size_t contentPos = line.size() - item->content.size();
        return line.substr(0, contentPos) + "[ ] " + item->content;
    }
    if (IsBlankOrSpaces(line)) {
        return line + "- [ ] ";
    }
    // Plain text line: convert to an unchecked task item, keeping indentation.
    const size_t indentLength = IndentLength(line);
    return line.substr(0, indentLength) + "- [ ] " + line.substr(indentLength);
}

std::vector<std::string> RenumberListBlock(const std::vector<std::string>& lines) {
    std::vector<std::string> result;
    result.reserve(lines.size());
    // Counter stack: (indent depth, next number, width, delimiter).
    struct Counter {
        size_t depth;
        int next;
        int width;
        char delimiter;
    };
    std::vector<Counter> counters;

    for (const auto& line : lines) {
        const auto item = ParseListItem(line);
        if (!item || item->isQuote || !item->isOrdered) {
            if (item && !item->isQuote && !item->isOrdered) {
                // Unordered item: reset deeper ordered counters at or below its depth.
                const size_t depth = IndentDepth(item->indent);
                while (!counters.empty() && counters.back().depth >= depth) {
                    counters.pop_back();
                }
            }
            result.push_back(line);
            continue;
        }
        const size_t depth = IndentDepth(item->indent);
        while (!counters.empty() && counters.back().depth > depth) {
            counters.pop_back();
        }
        int number;
        if (!counters.empty() && counters.back().depth == depth) {
            number = counters.back().next;
            counters.back().next = number + 1;
        } else {
            number = item->orderedNumber;  // first item of this depth keeps its start
            counters.push_back({depth, number + 1, item->orderedWidth, item->orderedDelimiter});
        }
        std::string numberText = std::to_string(number);
        const int width = counters.empty() ? item->orderedWidth : counters.back().width;
        while (static_cast<int>(numberText.size()) < width) {
            numberText.insert(numberText.begin(), '0');
        }
        // Rebuild the line with the new number, keeping everything else.
        std::string rebuilt = item->indent + numberText + item->orderedDelimiter + " ";
        if (item->isTask) {
            rebuilt += item->taskChecked ? "[x] " : "[ ] ";
        }
        rebuilt += item->content;
        result.push_back(std::move(rebuilt));
    }
    return result;
}

bool IsListBlockLine(const std::string& line) {
    if (ParseListItem(line)) {
        return true;
    }
    // Indented continuation of a list item.
    return !line.empty() && (line[0] == ' ' || line[0] == '\t') && !IsBlankOrSpaces(line);
}

}  // namespace nmf
