#include "core/MarkdownViewFeature.h"
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
    assert(rendered.bodyHtml.find("<h1>Title</h1>") != std::string::npos);
    assert(rendered.bodyHtml.find("<li>one</li>") != std::string::npos);
    assert(rendered.bodyHtml.find("<table>") != std::string::npos);
    assert(rendered.documentHtml.find("<base href=\"file:///C:/tmp/\">") != std::string::npos);
}

void TestHtmlEscape() {
    assert(nmf::EscapeHtmlText("<tag attr=\"&\">'") == "&lt;tag attr=&quot;&amp;&quot;&gt;&#39;");
}

void TestFeatureToggle() {
    bool hidden = false;
    int shown = 0;
    std::string html;
    nmf::MarkdownViewFeature feature(
        [](const nmf::ActiveDocument&) { return std::string("# Hello"); },
        [](const nmf::ActiveDocument&) { return 0.4; },
        [](const nmf::ActiveDocument&) { return 0.7; },
        [&](HWND, const std::string& rendered, const std::wstring&, double ratio) {
            ++shown;
            html = rendered;
            assert(ratio == 0.4);
        },
        [&]() {
            hidden = true;
            return 0.6;
        },
        [](const nmf::ActiveDocument&, double) {},
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
    assert(html.find("<h1>Hello</h1>") != std::string::npos);

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
    TestHtmlEscape();
    TestFeatureToggle();
    TestWebViewUserDataFolder();
    std::cout << "nmf_tests passed\n";
    return 0;
}
