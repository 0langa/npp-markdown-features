#pragma once

#include "core/Feature.h"
#include "rendering/MarkdownRenderer.h"

#include <functional>

namespace nmf {

class MarkdownViewFeature final : public Feature {
public:
    using ReadTextCallback = std::function<std::string(const ActiveDocument&)>;
    using ReadViewportCallback = std::function<double(const ActiveDocument&)>;
    using ShowHtmlCallback = std::function<void(HWND, const std::string&, const std::wstring&, double)>;
    using HideCallback = std::function<double()>;
    using SetViewportCallback = std::function<void(const ActiveDocument&, double)>;
    using StatusCallback = std::function<void(const std::wstring&)>;

    MarkdownViewFeature(
        ReadTextCallback readText,
        ReadViewportCallback readRawViewport,
        ReadViewportCallback readRenderedViewport,
        ShowHtmlCallback showHtml,
        HideCallback hide,
        SetViewportCallback setRawViewport,
        StatusCallback status);

    std::wstring Id() const override;
    std::wstring DisplayName() const override;
    void LoadSettings(const AppSettings& settings) override;
    void SaveSettings(AppSettings& settings) const override;
    void OnCommand(PluginCommand command, const ActiveDocument& document) override;
    void OnDocumentChanged(const ActiveDocument& document) override;
    void OnFileSaved(const ActiveDocument& document) override;

    bool IsRenderedMode() const;
    const MarkdownViewSettings& Settings() const;
    void UpdateSettings(MarkdownViewSettings settings);

private:
    void ApplyDocument(const ActiveDocument& document, bool forceRender);
    bool IsMarkdown(const ActiveDocument& document) const;

    MarkdownViewSettings settings_{};
    bool renderedMode_{false};
    double pendingRenderRatio_{-1.0};
    MarkdownRenderer renderer_{};
    ReadTextCallback readText_;
    ReadViewportCallback readRawViewport_;
    ReadViewportCallback readRenderedViewport_;
    ShowHtmlCallback showHtml_;
    HideCallback hide_;
    SetViewportCallback setRawViewport_;
    StatusCallback status_;
};

}  // namespace nmf
