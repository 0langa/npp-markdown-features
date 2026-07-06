#pragma once

#include <optional>
#include <string>
#include <vector>

namespace nmf {

enum class LinkKind {
    External,   // scheme:// or mailto:
    Anchor,     // #heading-slug
    LocalFile,  // anything else (relative or absolute path)
};

struct MarkdownLink {
    std::string text;      // link text (empty for autolinks/bare URLs)
    std::string target;    // raw destination (before %-decoding)
    std::string reference; // reference id for [text][id] style; empty otherwise
    bool isImage{false};
    size_t start{0};       // byte offsets within the line
    size_t end{0};
};

// Find the link containing byteColumn on this line: inline [t](u), image
// ![t](u), autolink <scheme:...>, reference [t][id] / [id][], or a bare
// http(s) URL. Returns the innermost/first match containing the column.
std::optional<MarkdownLink> LinkAtPosition(const std::string& line, size_t byteColumn);

// All links on a line, in order (same forms as LinkAtPosition).
std::vector<MarkdownLink> LinksOnLine(const std::string& line);

// Resolve "[id]: url" reference definitions (case-insensitive id).
std::optional<std::string> ResolveReference(const std::vector<std::string>& lines, const std::string& id);

LinkKind ClassifyLinkTarget(const std::string& target);

// Percent-decode a path-ish target and split off a trailing "#fragment".
struct LocalTarget {
    std::string path;
    std::string fragment;
};
LocalTarget SplitLocalTarget(const std::string& target);

// GitHub-style slug for matching "#fragment" anchors against heading text.
std::string GithubSlug(const std::string& headingText);

}  // namespace nmf
