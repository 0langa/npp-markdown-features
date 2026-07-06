#pragma once

#include "core/MarkdownOutline.h"

#include <string>

namespace nmf {

struct RenderedMarkdown {
    std::string bodyHtml;
    std::string documentHtml;
    MarkdownOutline outline;
};

class MarkdownRenderer {
public:
    RenderedMarkdown Render(const std::string& markdownUtf8, const std::wstring& sourcePath) const;

    // "auto" (follow system), "light", or "dark".
    void SetTheme(std::string theme);
    // Extra user CSS appended after the built-in stylesheet.
    void SetCustomCss(std::string cssUtf8);

private:
    std::string BuildDocument(const std::string& bodyHtml, const std::wstring& sourcePath) const;

    std::string theme_{"auto"};
    std::string customCss_{};
};

std::string EscapeHtmlText(const std::string& text);

}  // namespace nmf
