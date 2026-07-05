#pragma once

#include "core/Feature.h"
#include "core/ScrollSync.h"
#include "rendering/MarkdownRenderer.h"

#include <functional>
#include <optional>

namespace nmf {

class MarkdownViewFeature final : public Feature {
public:
    using ReadTextCallback = std::function<std::string(const ActiveDocument&)>;
    using ReadViewportCallback = std::function<double(const ActiveDocument&)>;
    using ReadFirstVisibleLineCallback = std::function<int(const ActiveDocument&)>;
    using ShowHtmlCallback = std::function<void(HWND, const std::string&, const std::wstring&, const ScrollTarget&)>;
    using HideCallback = std::function<ScrollTarget()>;
    using SetViewportCallback = std::function<void(const ActiveDocument&, const ScrollTarget&, const MarkdownOutline&)>;
    using StatusCallback = std::function<void(const std::wstring&)>;

    MarkdownViewFeature(
        ReadTextCallback readText,
        ReadViewportCallback readRawViewport,
        ReadViewportCallback readRenderedViewport,
        ReadFirstVisibleLineCallback readFirstVisibleLine,
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
    void ApplyDocument(const ActiveDocument& document, bool forceRender, std::optional<std::string> knownMarkdown);
    bool IsMarkdown(const ActiveDocument& document) const;

    MarkdownViewSettings settings_{};
    bool renderedMode_{false};
    ScrollTarget pendingRenderTarget_{};
    bool hasPendingRenderTarget_{false};
    MarkdownRenderer renderer_{};
    SectionScrollSyncStrategy scrollSync_{};
    MarkdownOutline currentOutline_{};
    ReadTextCallback readText_;
    ReadViewportCallback readRawViewport_;
    ReadViewportCallback readRenderedViewport_;
    ReadFirstVisibleLineCallback readFirstVisibleLine_;
    ShowHtmlCallback showHtml_;
    HideCallback hide_;
    SetViewportCallback setRawViewport_;
    StatusCallback status_;
};

}  // namespace nmf
