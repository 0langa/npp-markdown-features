#include "core/MarkdownViewFeature.h"
#include "core/ScrollSync.h"
#include "core/SettingsStore.h"
#include "core/Strings.h"
#include "rendering/MarkdownRenderer.h"
#include "rendering/WebViewUserDataFolder.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

void TestExtensionDetection() {
    const std::vector<std::wstring> extensions{L".md", L".markdown"};
    assert(nmf::EndsWithMarkdownExtension(L"C:\\tmp\\README.md", extensions));
    assert(nmf::EndsWithMarkdownExtension(L"C:\\tmp\\README.MD", extensions));
    assert(nmf::EndsWithMarkdownExtension(L"C:\\tmp\\notes.markdown", extensions));
    assert(!nmf::EndsWithMarkdownExtension(L"C:\\tmp\\notes.txt", extensions));
}

void TestSettingsDefaultsAndRoundTrip() {
    const auto path = std::filesystem::temp_directory_path() / L"nmf-settings-test.json";
    std::error_code ec;
    std::filesystem::remove(path, ec);

    nmf::SettingsStore store(path);
    auto settings = store.Load();
    assert(settings.schemaVersion == 1);
    assert(settings.markdownView.defaultMode == nmf::DisplayMode::Raw);
    assert(settings.markdownView.extensions.size() == 2);

    settings.markdownView.defaultMode = nmf::DisplayMode::Rendered;
    settings.markdownView.extensions = {L".md", L".mdown"};
    store.Save(settings);

    auto loaded = store.Load();
    assert(loaded.markdownView.defaultMode == nmf::DisplayMode::Rendered);
    assert(loaded.markdownView.extensions.size() == 2);
    assert(loaded.markdownView.extensions[1] == L".mdown");
    std::filesystem::remove(path, ec);
}

void TestMarkdownRender() {
    nmf::MarkdownRenderer renderer;
    const auto rendered = renderer.Render("# Title\n\n- one\n- two\n\n| A | B |\n| - | - |\n| 1 | 2 |\n\n```cpp\nint x = 1;\n```\n", L"C:\\tmp\\test.md");
    assert(rendered.bodyHtml.find("<h1 id=\"nmf-heading-title\" data-sourcepos=\"1:1-") != std::string::npos);
    assert(rendered.bodyHtml.find("one</li>") != std::string::npos);
    assert(rendered.bodyHtml.find("<table") != std::string::npos);
    assert(rendered.bodyHtml.find("language-cpp") != std::string::npos);
    assert(rendered.bodyHtml.find("<p data-sourcepos=") == std::string::npos);  // no loose paragraphs in this input
    assert(rendered.documentHtml.find("<base href=\"file:///C:/tmp/\">") != std::string::npos);
}

void TestMarkdownRenderSourcepos() {
    nmf::MarkdownRenderer renderer;
    const auto rendered = renderer.Render("# One\n\nParagraph here.\n\n> quote\n", L"");
    assert(rendered.bodyHtml.find("data-sourcepos=\"1:1-") != std::string::npos);
    assert(rendered.bodyHtml.find("<p data-sourcepos=\"3:1-") != std::string::npos);
    assert(rendered.bodyHtml.find("<blockquote data-sourcepos=\"5:1-") != std::string::npos);
}

void TestMarkdownRenderTaskListAndFootnotes() {
    nmf::MarkdownRenderer renderer;
    const auto tasks = renderer.Render("- [x] done\n- [ ] todo\n", L"");
    assert(tasks.bodyHtml.find("type=\"checkbox\"") != std::string::npos);
    assert(tasks.bodyHtml.find("checked") != std::string::npos);

    const auto notes = renderer.Render("Hi[^1]\n\n[^1]: A note\n", L"");
    assert(notes.bodyHtml.find("footnote") != std::string::npos);
}

void TestMarkdownOutline() {
    const auto outline = nmf::MarkdownOutline::Parse("# One\n\nText\n\n## Two\n\n```md\n# Ignored\n```\n\n# One\n");
    assert(outline.Headings().size() == 3);
    assert(outline.Headings()[0].anchorId == "nmf-heading-one");
    assert(outline.Headings()[1].anchorId == "nmf-heading-two");
    assert(outline.Headings()[2].anchorId == "nmf-heading-one-2");
    const auto heading = outline.HeadingAtOrBeforeLine(5);
    assert(heading);
    assert(heading->anchorId == "nmf-heading-two");
}

void TestMarkdownOutlineSetextAndInline() {
    const auto outline = nmf::MarkdownOutline::Parse("Setext Title\n============\n\nBody\n\nSecond\n------\n\n### Code `x` and *emph*\n");
    assert(outline.Headings().size() == 3);
    assert(outline.Headings()[0].level == 1);
    assert(outline.Headings()[0].line == 0);
    assert(outline.Headings()[0].text == "Setext Title");
    assert(outline.Headings()[1].level == 2);
    assert(outline.Headings()[1].line == 5);
    assert(outline.Headings()[2].text == "Code x and emph");
    assert(outline.Headings()[2].line == 8);
}

void TestHtmlEscape() {
    assert(nmf::EscapeHtmlText("<tag attr=\"&\">'") == "&lt;tag attr=&quot;&amp;&quot;&gt;&#39;");
}

void TestBlockScrollSync() {
    const auto outline = nmf::MarkdownOutline::Parse("# One\n\ntext\n\n## Two\n\nmore\n");
    const nmf::BlockScrollSyncStrategy sync;

    const auto target = sync.RawToRendered(outline, 5, 0.4);
    assert(target.sourceLine == 6.0);
    assert(target.anchorId == "nmf-heading-two");
    assert(target.ratio == 0.4);

    const nmf::ScrollTarget lineCaptured{0.9, "", 12.7};
    assert(sync.RenderedToRawLine(lineCaptured, outline) == 11);

    const nmf::ScrollTarget anchorOnly{0.9, "nmf-heading-two", -1.0};
    assert(sync.RenderedToRawLine(anchorOnly, outline) == 4);

    const nmf::ScrollTarget ratioOnly{0.9, "missing", -1.0};
    assert(sync.RenderedToRawLine(ratioOnly, outline) == -1);
}

void TestFeatureToggle() {
    bool hidden = false;
    int shown = 0;
    std::string html;
    nmf::ScrollTarget lastShownTarget{};
    nmf::MarkdownViewFeature feature(
        [](const nmf::ActiveDocument&) { return std::string("# Hello"); },
        [](const nmf::ActiveDocument&) { return 0.4; },
        [](const nmf::ActiveDocument&) { return nmf::ScrollTarget{0.55, "", 3.0}; },
        [](const nmf::ActiveDocument&) { return 0; },
        [&](HWND, const std::string& rendered, const std::wstring&, const nmf::ScrollTarget& scrollTarget) {
            ++shown;
            html = rendered;
            lastShownTarget = scrollTarget;
        },
        [&]() {
            hidden = true;
            return nmf::ScrollTarget{0.6, "nmf-heading-hello", 2.0};
        },
        [](const nmf::ActiveDocument&, const nmf::ScrollTarget&, const nmf::MarkdownOutline&) {},
        [](const std::wstring&) {});

    nmf::AppSettings settings;
    feature.LoadSettings(settings);
    nmf::ActiveDocument markdown{reinterpret_cast<HWND>(1), L"C:\\tmp\\a.md", true};
    nmf::ActiveDocument text{reinterpret_cast<HWND>(1), L"C:\\tmp\\a.txt", true};

    feature.OnDocumentChanged(markdown);
    assert(hidden);
    hidden = false;

    feature.OnCommand(nmf::PluginCommand::ToggleRenderedView, markdown);
    assert(feature.IsRenderedMode());
    assert(shown == 1);
    assert(html.find("Hello</h1>") != std::string::npos);
    assert(lastShownTarget.anchorId == "nmf-heading-hello");
    assert(lastShownTarget.sourceLine == 1.0);

    // Re-activating the same unchanged document must not re-render.
    feature.OnDocumentChanged(markdown);
    assert(shown == 1);

    // Saving re-renders the same document but keeps the rendered position.
    feature.OnFileSaved(markdown);
    assert(shown == 2);
    assert(lastShownTarget.sourceLine == 3.0);

    feature.OnDocumentChanged(text);
    assert(hidden);
}

void TestWebViewUserDataFolder() {
    const auto folder = nmf::DefaultWebViewUserDataFolder().wstring();
    assert(folder.find(L"NppMarkdownFeatures") != std::wstring::npos);
    assert(folder.find(L"WebView2") != std::wstring::npos);
    assert(folder.find(L"Program Files") == std::wstring::npos);
}

}  // namespace

int main() {
    TestExtensionDetection();
    TestSettingsDefaultsAndRoundTrip();
    TestMarkdownRender();
    TestMarkdownRenderSourcepos();
    TestMarkdownRenderTaskListAndFootnotes();
    TestMarkdownOutline();
    TestMarkdownOutlineSetextAndInline();
    TestHtmlEscape();
    TestBlockScrollSync();
    TestFeatureToggle();
    TestWebViewUserDataFolder();
    std::cout << "nmf_tests passed\n";
    return 0;
}
