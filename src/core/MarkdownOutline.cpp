#include "core/MarkdownOutline.h"

#include <algorithm>
#include <cctype>
#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm.h>
#include <map>

namespace nmf {
namespace {

std::string TrimAscii(std::string value) {
    const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::string HeadingPlainText(cmark_node* heading) {
    std::string text;
    cmark_iter* iter = cmark_iter_new(heading);
    cmark_event_type event;
    while ((event = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        if (event != CMARK_EVENT_ENTER) {
            continue;
        }
        cmark_node* node = cmark_iter_get_node(iter);
        switch (cmark_node_get_type(node)) {
            case CMARK_NODE_TEXT:
            case CMARK_NODE_CODE:
                if (const char* literal = cmark_node_get_literal(node)) {
                    text += literal;
                }
                break;
            case CMARK_NODE_SOFTBREAK:
            case CMARK_NODE_LINEBREAK:
                text += ' ';
                break;
            default:
                break;
        }
    }
    cmark_iter_free(iter);
    return TrimAscii(text);
}

}  // namespace

// Parse with the same cmark-gfm configuration as the renderer so heading
// lines always match the data-sourcepos values in the rendered HTML. This
// also handles setext headings, fences, and CRLF exactly like the renderer.
MarkdownOutline MarkdownOutline::Parse(const std::string& markdownUtf8) {
    MarkdownOutline outline;
    std::map<std::string, int> anchorCounts;

    cmark_gfm_core_extensions_ensure_registered();
    cmark_parser* parser = cmark_parser_new(CMARK_OPT_VALIDATE_UTF8 | CMARK_OPT_FOOTNOTES);
    if (parser == nullptr) {
        return outline;
    }
    for (const char* extensionName : {"table", "strikethrough", "tasklist", "autolink"}) {
        if (cmark_syntax_extension* extension = cmark_find_syntax_extension(extensionName)) {
            cmark_parser_attach_syntax_extension(parser, extension);
        }
    }
    cmark_parser_feed(parser, markdownUtf8.data(), markdownUtf8.size());
    cmark_node* document = cmark_parser_finish(parser);
    if (document == nullptr) {
        cmark_parser_free(parser);
        return outline;
    }

    cmark_iter* iter = cmark_iter_new(document);
    cmark_event_type event;
    while ((event = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node* node = cmark_iter_get_node(iter);
        if (event != CMARK_EVENT_ENTER || cmark_node_get_type(node) != CMARK_NODE_HEADING) {
            continue;
        }
        const auto text = HeadingPlainText(node);
        if (text.empty()) {
            continue;
        }
        const int level = cmark_node_get_heading_level(node);
        const int line = cmark_node_get_start_line(node) - 1;  // outline lines are 0-based
        const auto base = SlugifyHeading(text, 0);
        const int duplicate = anchorCounts[base]++;
        outline.headings_.push_back({line, level, text, SlugifyHeading(text, duplicate)});
    }
    cmark_iter_free(iter);
    cmark_node_free(document);
    cmark_parser_free(parser);
    return outline;
}

const std::vector<MarkdownHeading>& MarkdownOutline::Headings() const {
    return headings_;
}

std::optional<MarkdownHeading> MarkdownOutline::HeadingAtOrBeforeLine(int line) const {
    std::optional<MarkdownHeading> result;
    for (const auto& heading : headings_) {
        if (heading.line > line) {
            break;
        }
        result = heading;
    }
    return result;
}

std::optional<MarkdownHeading> MarkdownOutline::HeadingByAnchor(const std::string& anchorId) const {
    const auto it = std::find_if(headings_.begin(), headings_.end(), [&](const MarkdownHeading& heading) { return heading.anchorId == anchorId; });
    if (it == headings_.end()) {
        return std::nullopt;
    }
    return *it;
}

std::string SlugifyHeading(const std::string& headingText, int duplicateIndex) {
    std::string slug;
    bool previousDash = false;
    for (unsigned char ch : headingText) {
        if (std::isalnum(ch) != 0) {
            slug.push_back(static_cast<char>(std::tolower(ch)));
            previousDash = false;
        } else if ((std::isspace(ch) != 0 || ch == '-' || ch == '_') && !slug.empty() && !previousDash) {
            slug.push_back('-');
            previousDash = true;
        }
    }
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    if (slug.empty()) {
        slug = "section";
    }
    if (duplicateIndex > 0) {
        slug += "-" + std::to_string(duplicateIndex + 1);
    }
    return "nmf-heading-" + slug;
}

}  // namespace nmf
