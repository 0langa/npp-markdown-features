#include "core/MarkdownViewFeature.h"

#include "core/Strings.h"

#include <utility>

namespace nmf {

MarkdownViewFeature::MarkdownViewFeature(
    ReadTextCallback readText,
    ReadViewportCallback readRawViewport,
    ReadViewportCallback readRenderedViewport,
    ShowHtmlCallback showHtml,
    HideCallback hide,
    SetViewportCallback setRawViewport,
    StatusCallback status)
    : readText_(std::move(readText)),
      readRawViewport_(std::move(readRawViewport)),
      readRenderedViewport_(std::move(readRenderedViewport)),
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
            const double ratio = hide_();
            renderedMode_ = false;
            if (IsMarkdown(document)) {
                setRawViewport_(document, ratio);
            }
            status_(L"Markdown Features: raw view");
            return;
        }
        pendingRenderRatio_ = readRawViewport_(document);
        renderedMode_ = true;
        ApplyDocument(document, true);
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
    if (!renderedMode_ || !IsMarkdown(document) || document.scintilla == nullptr) {
        hide_();
        status_(L"Markdown Features: raw view");
        return;
    }

    if (!forceRender) {
        return;
    }

    try {
        const auto markdown = readText_(document);
        const auto rendered = renderer_.Render(markdown, document.path);
        const double ratio = pendingRenderRatio_ >= 0.0 ? pendingRenderRatio_ : readRenderedViewport_(document);
        pendingRenderRatio_ = -1.0;
        showHtml_(document.scintilla, rendered.documentHtml, document.path, ratio);
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
