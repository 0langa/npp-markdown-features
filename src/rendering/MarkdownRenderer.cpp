#include "rendering/MarkdownRenderer.h"

#include "core/Strings.h"

#include <filesystem>
#include <md4c-html.h>
#include <stdexcept>

namespace nmf {
namespace {

void AppendHtml(const MD_CHAR* text, MD_SIZE size, void* userData) {
    auto* output = static_cast<std::string*>(userData);
    output->append(text, text + size);
}

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

std::string AddHeadingAnchors(std::string body, const MarkdownOutline& outline) {
    size_t searchFrom = 0;
    for (const auto& heading : outline.Headings()) {
        const std::string tag = "<h" + std::to_string(heading.level) + ">";
        const auto position = body.find(tag, searchFrom);
        if (position == std::string::npos) {
            continue;
        }
        const std::string replacement = "<h" + std::to_string(heading.level) + " id=\"" + EscapeHtmlText(heading.anchorId) + "\" data-source-line=\"" + std::to_string(heading.line) + "\">";
        body.replace(position, tag.size(), replacement);
        searchFrom = position + replacement.size();
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
    std::string body;
    constexpr unsigned parserFlags = MD_DIALECT_GITHUB | MD_FLAG_TABLES | MD_FLAG_TASKLISTS | MD_FLAG_STRIKETHROUGH;
    constexpr unsigned rendererFlags = MD_HTML_FLAG_SKIP_UTF8_BOM;
    const int result = md_html(markdownUtf8.data(), static_cast<MD_SIZE>(markdownUtf8.size()), AppendHtml, &body, parserFlags, rendererFlags);
    if (result != 0) {
        throw std::runtime_error("Markdown rendering failed");
    }
    const auto outline = MarkdownOutline::Parse(markdownUtf8);
    body = AddHeadingAnchors(std::move(body), outline);
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
@media (prefers-color-scheme: dark) {
  body { color: #e6e6e6; background: #1f1f1f; }
  a { color: #8ab4f8; }
  pre, code { background: #2b2f36; }
  th, td, h1, pre { border-color: #454b55; }
  blockquote { border-left-color: #5f6670; color: #c8c8c8; }
}
</style></head><body>)";
    document += bodyHtml;
    document += "</body></html>";
    return document;
}

}  // namespace nmf
