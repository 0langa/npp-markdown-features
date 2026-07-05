#include "core/MarkdownViewFeature.h"

#include "core/Strings.h"

#include <utility>

namespace nmf {

MarkdownViewFeature::MarkdownViewFeature(
    ReadTextCallback readText,
    ReadViewportCallback readRawViewport,
    ReadViewportCallback readRenderedViewport,
    ReadFirstVisibleLineCallback readFirstVisibleLine,
    ShowHtmlCallback showHtml,
    HideCallback hide,
    SetViewportCallback setRawViewport,
    StatusCallback status)
    : readText_(std::move(readText)),
      readRawViewport_(std::move(readRawViewport)),
      readRenderedViewport_(std::move(readRenderedViewport)),
      readFirstVisibleLine_(std::move(readFirstVisibleLine)),
      showHtml_(std::move(showHtml)),
      hide_(std::move(hide)),
      setRawViewport_(std::move(setRawViewport)),
      status_(std::move(status)) {}

std::wstring MarkdownViewFeature::Id() const {
    return L"markdownView";
}

std::wstring MarkdownViewFeature::DisplayName() const {
    return L"Markdown View";
}

void MarkdownViewFeature::LoadSettings(const AppSettings& settings) {
    settings_ = settings.markdownView;
    renderedMode_ = settings_.defaultMode == DisplayMode::Rendered;
}

void MarkdownViewFeature::SaveSettings(AppSettings& settings) const {
    settings.markdownView = settings_;
    settings.markdownView.defaultMode = renderedMode_ ? DisplayMode::Rendered : DisplayMode::Raw;
}

void MarkdownViewFeature::OnCommand(PluginCommand command, const ActiveDocument& document) {
    if (command == PluginCommand::ToggleRenderedView) {
        if (renderedMode_) {
            const auto target = hide_();
            renderedMode_ = false;
            if (IsMarkdown(document)) {
                setRawViewport_(document, target, currentOutline_);
            }
            status_(L"Markdown Features: raw view");
            return;
        }

        const auto markdown = readText_(document);
        currentOutline_ = MarkdownOutline::Parse(markdown);
        pendingRenderTarget_ = scrollSync_.RawToRendered(currentOutline_, readFirstVisibleLine_(document), readRawViewport_(document));
        hasPendingRenderTarget_ = true;
        renderedMode_ = true;
        ApplyDocument(document, true, markdown);
        return;
    }
    if (command == PluginCommand::RefreshRenderedView) {
        ApplyDocument(document, true);
    }
}

void MarkdownViewFeature::OnDocumentChanged(const ActiveDocument& document) {
    ApplyDocument(document, false);
}

void MarkdownViewFeature::OnFileSaved(const ActiveDocument& document) {
    ApplyDocument(document, true);
}

bool MarkdownViewFeature::IsRenderedMode() const {
    return renderedMode_;
}

const MarkdownViewSettings& MarkdownViewFeature::Settings() const {
    return settings_;
}

void MarkdownViewFeature::UpdateSettings(MarkdownViewSettings settings) {
    settings_ = std::move(settings);
    renderedMode_ = settings_.defaultMode == DisplayMode::Rendered;
}

void MarkdownViewFeature::ApplyDocument(const ActiveDocument& document, bool forceRender) {
    ApplyDocument(document, forceRender, std::nullopt);
}

void MarkdownViewFeature::ApplyDocument(const ActiveDocument& document, bool forceRender, std::optional<std::string> knownMarkdown) {
    if (!renderedMode_ || !IsMarkdown(document) || document.scintilla == nullptr) {
        hide_();
        status_(L"Markdown Features: raw view");
        return;
    }

    if (!forceRender) {
        return;
    }

    try {
        const auto markdown = knownMarkdown ? *knownMarkdown : readText_(document);
        const auto rendered = renderer_.Render(markdown, document.path);
        currentOutline_ = rendered.outline;
        const auto target = hasPendingRenderTarget_
            ? pendingRenderTarget_
            : scrollSync_.RawToRendered(currentOutline_, readFirstVisibleLine_(document), readRenderedViewport_(document));
        hasPendingRenderTarget_ = false;
        showHtml_(document.scintilla, rendered.documentHtml, document.path, target);
        status_(L"Markdown Features: rendered view");
    } catch (...) {
        hide_();
        status_(L"Markdown Features: render failed");
    }
}

bool MarkdownViewFeature::IsMarkdown(const ActiveDocument& document) const {
    return EndsWithMarkdownExtension(document.path, settings_.extensions);
}

}  // namespace nmf
