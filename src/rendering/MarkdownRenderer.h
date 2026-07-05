#pragma once

#include <string>

namespace nmf {

struct RenderedMarkdown {
    std::string bodyHtml;
    std::string documentHtml;
};

class MarkdownRenderer {
public:
    RenderedMarkdown Render(const std::string& markdownUtf8, const std::wstring& sourcePath) const;

private:
    static std::string BuildDocument(const std::string& bodyHtml, const std::wstring& sourcePath);
};

std::string EscapeHtmlText(const std::string& text);

}  // namespace nmf
