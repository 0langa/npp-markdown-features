#include "rendering/MarkdownRenderer.h"

#include "core/DocumentStats.h"
#include "core/Strings.h"

#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm.h>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace nmf {
namespace {

std::string FileUriFromDirectory(const std::wstring& sourcePath) {
    if (sourcePath.empty()) {
        return {};
    }
    std::filesystem::path path(sourcePath);
    const auto directory = path.has_parent_path() ? path.parent_path() : path;
    auto native = directory.wstring();
    std::replace(native.begin(), native.end(), L'\\', L'/');
    std::string uri = WideToUtf8(native);
    if (uri.empty()) {
        return {};
    }
    if (uri.back() != '/') {
        uri.push_back('/');
    }
    return "file:///" + uri;
}

// cmark emits data-sourcepos="<startline>:<startcol>-<endline>:<endcol>" with
// 1-based lines; outline lines are 0-based, so heading.line + 1 keys the match.
std::string AddHeadingAnchors(std::string body, const MarkdownOutline& outline) {
    size_t searchFrom = 0;
    for (const auto& heading : outline.Headings()) {
        const std::string openTag = "<h" + std::to_string(heading.level);
        const std::string tag = openTag + " data-sourcepos=\"" + std::to_string(heading.line + 1) + ":";
        const auto position = body.find(tag, searchFrom);
        if (position == std::string::npos) {
            continue;
        }
        const std::string idAttribute = " id=\"" + EscapeHtmlText(heading.anchorId) + "\"";
        body.insert(position + openTag.size(), idAttribute);
        searchFrom = position + tag.size() + idAttribute.size();
    }
    return body;
}

}  // namespace

std::string EscapeHtmlText(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char ch : text) {
        switch (ch) {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '\'':
                escaped += "&#39;";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }
    return escaped;
}

RenderedMarkdown MarkdownRenderer::Render(const std::string& markdownUtf8, const std::wstring& sourcePath) const {
    cmark_gfm_core_extensions_ensure_registered();

    // YAML frontmatter would render as a broken thematic break / setext mess.
    // Blank those lines (keeping the line count identical so data-sourcepos
    // stays aligned with the editor) and show the raw text in a collapsible
    // block instead.
    std::string frontmatterText;
    std::string parseInput = markdownUtf8;
    {
        const auto lines = SplitLines(markdownUtf8);
        const auto frontmatter = DetectFrontmatter(lines);
        if (frontmatter.present) {
            std::string blanked;
            std::string raw;
            for (int index = 0; index < static_cast<int>(lines.size()); ++index) {
                const bool inFrontmatter = index <= frontmatter.endLine;
                if (inFrontmatter && index > 0 && index < frontmatter.endLine) {
                    raw += lines[static_cast<size_t>(index)];
                    raw += '\n';
                }
                if (index > 0) {
                    blanked += '\n';
                }
                if (!inFrontmatter) {
                    blanked += lines[static_cast<size_t>(index)];
                }
            }
            frontmatterText = raw;
            parseInput = std::move(blanked);
        }
    }

    const int options = CMARK_OPT_SOURCEPOS | CMARK_OPT_UNSAFE | CMARK_OPT_FOOTNOTES | CMARK_OPT_VALIDATE_UTF8;
    cmark_parser* parser = cmark_parser_new(options);
    if (parser == nullptr) {
        throw std::runtime_error("Markdown parser could not be created");
    }
    for (const char* extensionName : {"table", "strikethrough", "tasklist", "autolink"}) {
        if (cmark_syntax_extension* extension = cmark_find_syntax_extension(extensionName)) {
            cmark_parser_attach_syntax_extension(parser, extension);
        }
    }
    cmark_parser_feed(parser, parseInput.data(), parseInput.size());
    cmark_node* document = cmark_parser_finish(parser);
    if (document == nullptr) {
        cmark_parser_free(parser);
        throw std::runtime_error("Markdown rendering failed");
    }
    char* html = cmark_render_html(document, options, cmark_parser_get_syntax_extensions(parser));
    std::string body(html != nullptr ? html : "");
    std::free(html);
    cmark_node_free(document);
    cmark_parser_free(parser);

    const auto outline = MarkdownOutline::Parse(parseInput);
    body = AddHeadingAnchors(std::move(body), outline);
    if (!frontmatterText.empty()) {
        body = "<details class=\"nmf-frontmatter\"><summary>Front matter</summary><pre>" + EscapeHtmlText(frontmatterText) + "</pre></details>\n" + body;
    }
    return {body, BuildDocument(body, sourcePath), outline};
}

std::string MarkdownRenderer::BuildDocument(const std::string& bodyHtml, const std::wstring& sourcePath) {
    const auto baseUri = FileUriFromDirectory(sourcePath);
    std::string document;
    document.reserve(bodyHtml.size() + 4096);
    document += "<!doctype html><html><head><meta charset=\"utf-8\">";
    if (!baseUri.empty()) {
        document += "<base href=\"";
        document += EscapeHtmlText(baseUri);
        document += "\">";
    }
    document += R"(<style>
:root { color-scheme: light dark; }
body {
  box-sizing: border-box;
  max-width: 980px;
  margin: 0 auto;
  padding: 28px 36px 64px;
  font-family: "Segoe UI", system-ui, sans-serif;
  font-size: 15px;
  line-height: 1.55;
  color: #202124;
  background: #ffffff;
}
h1, h2, h3, h4 { line-height: 1.25; margin: 1.25em 0 .55em; }
h1 { border-bottom: 1px solid #d7dce2; padding-bottom: .25em; }
pre, code { font-family: Consolas, "Cascadia Mono", monospace; }
pre { overflow: auto; padding: 12px 14px; background: #f5f7f9; border: 1px solid #d7dce2; border-radius: 6px; }
code { background: #f5f7f9; padding: .1em .3em; border-radius: 4px; }
pre code { padding: 0; background: transparent; }
table { border-collapse: collapse; width: 100%; margin: 1em 0; }
th, td { border: 1px solid #d7dce2; padding: 6px 9px; }
blockquote { margin-left: 0; padding-left: 1em; border-left: 4px solid #c7ced8; color: #555; }
img { max-width: 100%; height: auto; }
a { color: #0b57d0; }
li.task-list-item { list-style-type: none; }
li.task-list-item input[type="checkbox"] { margin: 0 .45em .1em -1.35em; vertical-align: middle; }
section.footnotes { margin-top: 2em; padding-top: .75em; border-top: 1px solid #d7dce2; font-size: .92em; }
details.nmf-frontmatter { margin: 0 0 1.25em; padding: .4em .8em; background: #f5f7f9; border: 1px solid #d7dce2; border-radius: 6px; font-size: .9em; }
details.nmf-frontmatter summary { cursor: pointer; color: #555; }
details.nmf-frontmatter pre { margin: .5em 0 .25em; border: none; background: transparent; padding: 0; }
@media (prefers-color-scheme: dark) {
  body { color: #e6e6e6; background: #1f1f1f; }
  a { color: #8ab4f8; }
  pre, code { background: #2b2f36; }
  th, td, h1, pre { border-color: #454b55; }
  blockquote { border-left-color: #5f6670; color: #c8c8c8; }
  section.footnotes { border-top-color: #454b55; }
  details.nmf-frontmatter { background: #2b2f36; border-color: #454b55; }
  details.nmf-frontmatter summary { color: #b8b8b8; }
}
</style></head><body>)";
    document += bodyHtml;
    document += "</body></html>";
    return document;
}

}  // namespace nmf
