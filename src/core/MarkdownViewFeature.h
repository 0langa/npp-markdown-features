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
    using ReadRenderedTargetCallback = std::function<ScrollTarget(const ActiveDocument&)>;
    using ReadFirstVisibleLineCallback = std::function<int(const ActiveDocument&)>;
    using ShowHtmlCallback = std::function<void(HWND, const std::string&, const std::wstring&, const ScrollTarget&)>;
    using HideCallback = std::function<ScrollTarget()>;
    using SetViewportCallback = std::function<void(const ActiveDocument&, const ScrollTarget&, const MarkdownOutline&)>;
    using StatusCallback = std::function<void(const std::wstring&)>;

    MarkdownViewFeature(
        ReadTextCallback readText,
        ReadViewportCallback readRawViewport,
        ReadRenderedTargetCallback readRenderedTarget,
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
    const BlockScrollSyncStrategy& ScrollSync() const;

private:
    enum class RenderReason {
        Toggle,
        DocumentChanged,
        Refresh,
    };

    void ApplyDocument(const ActiveDocument& document, RenderReason reason, std::optional<std::string> knownMarkdown = std::nullopt);
    bool IsMarkdown(const ActiveDocument& document) const;

    MarkdownViewSettings settings_{};
    bool renderedMode_{false};
    ScrollTarget pendingRenderTarget_{};
    bool hasPendingRenderTarget_{false};
    MarkdownRenderer renderer_{};
    BlockScrollSyncStrategy scrollSync_{};
    MarkdownOutline currentOutline_{};
    std::wstring lastRenderedPath_{};
    size_t lastRenderedHash_{0};
    bool hasRenderedDocument_{false};
    ReadTextCallback readText_;
    ReadViewportCallback readRawViewport_;
    ReadRenderedTargetCallback readRenderedTarget_;
    ReadFirstVisibleLineCallback readFirstVisibleLine_;
    ShowHtmlCallback showHtml_;
    HideCallback hide_;
    SetViewportCallback setRawViewport_;
    StatusCallback status_;
};

}  // namespace nmf
