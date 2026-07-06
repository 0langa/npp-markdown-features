#include "core/LinkTools.h"

#include <algorithm>
#include <cctype>

namespace nmf {
namespace {

bool IsSchemeChar(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '+' || ch == '-' || ch == '.';
}

bool HasScheme(const std::string& target) {
    if (target.empty() || std::isalpha(static_cast<unsigned char>(target[0])) == 0) {
        return false;
    }
    size_t index = 1;
    while (index < target.size() && IsSchemeChar(target[index])) {
        ++index;
    }
    return index + 2 < target.size() && target[index] == ':' && target[index + 1] == '/' && target[index + 2] == '/';
}

std::string ToLowerAscii(std::string value) {
    for (auto& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

// Parses one inline link/image starting at line[open] == '['. Returns the
// populated link (without kind classification) or nullopt.
std::optional<MarkdownLink> ParseBracketLink(const std::string& line, size_t open, bool isImage, size_t anchorStart) {
    int depth = 1;
    size_t close = open + 1;
    while (close < line.size() && depth > 0) {
        if (line[close] == '\\') {
            close += 2;
            continue;
        }
        if (line[close] == '[') {
            ++depth;
        } else if (line[close] == ']') {
            --depth;
            if (depth == 0) {
                break;
            }
        }
        ++close;
    }
    if (close >= line.size() || depth != 0) {
        return std::nullopt;
    }
    MarkdownLink link;
    link.isImage = isImage;
    link.text = line.substr(open + 1, close - open - 1);
    link.start = anchorStart;

    if (close + 1 < line.size() && line[close + 1] == '(') {
        // Inline destination, balanced parentheses, optional "title".
        int parens = 1;
        size_t endParen = close + 2;
        while (endParen < line.size() && parens > 0) {
            if (line[endParen] == '\\') {
                endParen += 2;
                continue;
            }
            if (line[endParen] == '(') {
                ++parens;
            } else if (line[endParen] == ')') {
                --parens;
                if (parens == 0) {
                    break;
                }
            }
            ++endParen;
        }
        if (endParen >= line.size() || parens != 0) {
            return std::nullopt;
        }
        std::string destination = line.substr(close + 2, endParen - close - 2);
        // Strip optional title: URL ends at first whitespace unless <wrapped>.
        if (!destination.empty() && destination.front() == '<') {
            const auto closing = destination.find('>');
            if (closing != std::string::npos) {
                destination = destination.substr(1, closing - 1);
            }
        } else {
            const auto space = destination.find_first_of(" \t");
            if (space != std::string::npos) {
                destination = destination.substr(0, space);
            }
        }
        link.target = destination;
        link.end = endParen + 1;
        return link;
    }
    if (close + 1 < line.size() && line[close + 1] == '[') {
        const auto refClose = line.find(']', close + 2);
        if (refClose == std::string::npos) {
            return std::nullopt;
        }
        link.reference = line.substr(close + 2, refClose - close - 2);
        if (link.reference.empty()) {
            link.reference = link.text;  // collapsed [id][]
        }
        link.end = refClose + 1;
        return link;
    }
    return std::nullopt;
}

}  // namespace

std::vector<MarkdownLink> LinksOnLine(const std::string& line) {
    std::vector<MarkdownLink> links;
    size_t index = 0;
    while (index < line.size()) {
        const char ch = line[index];
        if (ch == '\\') {
            index += 2;
            continue;
        }
        if (ch == '!' && index + 1 < line.size() && line[index + 1] == '[') {
            if (auto link = ParseBracketLink(line, index + 1, true, index)) {
                index = link->end;
                links.push_back(std::move(*link));
                continue;
            }
        }
        if (ch == '[') {
            if (auto link = ParseBracketLink(line, index, false, index)) {
                index = link->end;
                links.push_back(std::move(*link));
                continue;
            }
        }
        if (ch == '<') {
            const auto close = line.find('>', index + 1);
            if (close != std::string::npos) {
                const auto inner = line.substr(index + 1, close - index - 1);
                if (HasScheme(inner) || inner.rfind("mailto:", 0) == 0) {
                    MarkdownLink link;
                    link.target = inner;
                    link.start = index;
                    link.end = close + 1;
                    links.push_back(std::move(link));
                    index = close + 1;
                    continue;
                }
            }
        }
        if (line.compare(index, 7, "http://") == 0 || line.compare(index, 8, "https://") == 0) {
            size_t end = index;
            while (end < line.size() && line[end] != ' ' && line[end] != '\t' && line[end] != '<' && line[end] != '>' && line[end] != ')' && line[end] != ']') {
                ++end;
            }
            while (end > index && std::string(".,;:!?").find(line[end - 1]) != std::string::npos) {
                --end;
            }
            MarkdownLink link;
            link.target = line.substr(index, end - index);
            link.start = index;
            link.end = end;
            links.push_back(std::move(link));
            index = end;
            continue;
        }
        ++index;
    }
    return links;
}

std::optional<MarkdownLink> LinkAtPosition(const std::string& line, size_t byteColumn) {
    for (const auto& link : LinksOnLine(line)) {
        if (byteColumn >= link.start && byteColumn <= link.end) {
            return link;
        }
    }
    return std::nullopt;
}

std::optional<std::string> ResolveReference(const std::vector<std::string>& lines, const std::string& id) {
    const auto wanted = ToLowerAscii(id);
    for (const auto& line : lines) {
        size_t index = 0;
        while (index < line.size() && (line[index] == ' ' || line[index] == '\t')) {
            ++index;
        }
        if (index >= line.size() || line[index] != '[') {
            continue;
        }
        const auto close = line.find("]:", index + 1);
        if (close == std::string::npos) {
            continue;
        }
        if (ToLowerAscii(line.substr(index + 1, close - index - 1)) != wanted) {
            continue;
        }
        size_t urlStart = close + 2;
        while (urlStart < line.size() && (line[urlStart] == ' ' || line[urlStart] == '\t')) {
            ++urlStart;
        }
        size_t urlEnd = urlStart;
        while (urlEnd < line.size() && line[urlEnd] != ' ' && line[urlEnd] != '\t') {
            ++urlEnd;
        }
        if (urlEnd > urlStart) {
            auto url = line.substr(urlStart, urlEnd - urlStart);
            if (url.size() >= 2 && url.front() == '<' && url.back() == '>') {
                url = url.substr(1, url.size() - 2);
            }
            return url;
        }
    }
    return std::nullopt;
}

LinkKind ClassifyLinkTarget(const std::string& target) {
    if (target.empty()) {
        return LinkKind::LocalFile;
    }
    if (target[0] == '#') {
        return LinkKind::Anchor;
    }
    if (HasScheme(target) || ToLowerAscii(target).rfind("mailto:", 0) == 0) {
        return LinkKind::External;
    }
    return LinkKind::LocalFile;
}

LocalTarget SplitLocalTarget(const std::string& target) {
    LocalTarget result;
    const auto hash = target.find('#');
    std::string path = hash == std::string::npos ? target : target.substr(0, hash);
    if (hash != std::string::npos) {
        result.fragment = target.substr(hash + 1);
    }
    // Percent-decode the path portion.
    std::string decoded;
    decoded.reserve(path.size());
    for (size_t index = 0; index < path.size(); ++index) {
        if (path[index] == '%' && index + 2 < path.size()
            && std::isxdigit(static_cast<unsigned char>(path[index + 1])) != 0
            && std::isxdigit(static_cast<unsigned char>(path[index + 2])) != 0) {
            decoded.push_back(static_cast<char>(std::stoi(path.substr(index + 1, 2), nullptr, 16)));
            index += 2;
        } else {
            decoded.push_back(path[index]);
        }
    }
    result.path = decoded;
    return result;
}

std::string GithubSlug(const std::string& headingText) {
    std::string slug;
    for (unsigned char ch : headingText) {
        if (std::isalnum(ch) != 0) {
            slug.push_back(static_cast<char>(std::tolower(ch)));
        } else if (ch == ' ' || ch == '-' || ch == '_') {
            slug.push_back(ch == '_' ? '_' : '-');
        }
        // Other punctuation is dropped, matching GitHub's anchor rules
        // closely enough for ASCII headings.
    }
    return slug;
}

}  // namespace nmf
