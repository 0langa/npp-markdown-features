#include "core/MarkdownViewFeature.h"

#include "core/Strings.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <utility>

namespace {

std::string ReadFileUtf8(const std::wstring& path) {
    if (path.empty()) {
        return {};
    }
    std::ifstream input(std::filesystem::path(path), std::ios::binary);
    if (!input) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

}  // namespace

namespace nmf {

MarkdownViewFeature::MarkdownViewFeature(
    ReadTextCallback readText,
    ReadViewportCallback readRawViewport,
    ReadRenderedTargetCallback readRenderedTarget,
    ReadFirstVisibleLineCallback readFirstVisibleLine,
    ShowHtmlCallback showHtml,
    HideCallback hide,
    SetViewportCallback setRawViewport,
    StatusCallback status)
    : readText_(std::move(readText)),
      readRawViewport_(std::move(readRawViewport)),
      readRenderedTarget_(std::move(readRenderedTarget)),
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
    ApplyRendererSettings();
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
            hasRenderedDocument_ = false;
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
        ApplyDocument(document, RenderReason::Toggle, markdown);
        return;
    }
    if (command == PluginCommand::RefreshRenderedView) {
        ApplyDocument(document, RenderReason::Refresh);
    }
}

void MarkdownViewFeature::OnDocumentChanged(const ActiveDocument& document) {
    ApplyDocument(document, RenderReason::DocumentChanged);
}

void MarkdownViewFeature::OnFileSaved(const ActiveDocument& document) {
    ApplyDocument(document, RenderReason::Refresh);
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
    ApplyRendererSettings();
    hasRenderedDocument_ = false;  // force a re-render with the new style
}

void MarkdownViewFeature::ApplyRendererSettings() {
    renderer_.SetTheme(WideToUtf8(settings_.theme));
    renderer_.SetCustomCss(ReadFileUtf8(settings_.customCssPath));
}

const BlockScrollSyncStrategy& MarkdownViewFeature::ScrollSync() const {
    return scrollSync_;
}

void MarkdownViewFeature::ApplyDocument(const ActiveDocument& document, RenderReason reason, std::optional<std::string> knownMarkdown) {
    if (!renderedMode_ || !IsMarkdown(document) || document.scintilla == nullptr) {
        hide_();
        hasRenderedDocument_ = false;
        status_(L"Markdown Features: raw view");
        return;
    }

    try {
        const auto markdown = knownMarkdown ? *knownMarkdown : readText_(document);
        const auto markdownHash = std::hash<std::string>{}(markdown);
        if (reason == RenderReason::DocumentChanged && hasRenderedDocument_ && document.path == lastRenderedPath_ && markdownHash == lastRenderedHash_) {
            return;
        }

        const auto rendered = renderer_.Render(markdown, document.path);
        currentOutline_ = rendered.outline;

        ScrollTarget target;
        if (hasPendingRenderTarget_) {
            target = pendingRenderTarget_;
        } else if (reason == RenderReason::Refresh && hasRenderedDocument_ && document.path == lastRenderedPath_) {
            // Same document re-rendered: keep the rendered view where it is.
            target = readRenderedTarget_(document);
        } else {
            // New document under the overlay: mirror its raw editor position.
            target = scrollSync_.RawToRendered(currentOutline_, readFirstVisibleLine_(document), readRawViewport_(document));
        }
        hasPendingRenderTarget_ = false;

        showHtml_(document.scintilla, rendered.documentHtml, document.path, target);
        lastRenderedPath_ = document.path;
        lastRenderedHash_ = markdownHash;
        hasRenderedDocument_ = true;
        status_(L"Markdown Features: rendered view");
    } catch (...) {
        hide_();
        hasRenderedDocument_ = false;
        status_(L"Markdown Features: render failed");
    }
}

bool MarkdownViewFeature::IsMarkdown(const ActiveDocument& document) const {
    return EndsWithMarkdownExtension(document.path, settings_.extensions);
}

}  // namespace nmf
