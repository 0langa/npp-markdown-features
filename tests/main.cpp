#include "core/ListEditing.h"
#include "core/MarkdownViewFeature.h"
#include "core/TableEditing.h"
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

void TestListItemParsing() {
    auto bullet = nmf::ParseListItem("  - item text");
    assert(bullet && !bullet->isOrdered && !bullet->isTask && !bullet->isQuote);
    assert(bullet->indent == "  ");
    assert(bullet->bulletChar == '-');
    assert(bullet->content == "item text");

    auto ordered = nmf::ParseListItem("12) content");
    assert(ordered && ordered->isOrdered);
    assert(ordered->orderedNumber == 12);
    assert(ordered->orderedDelimiter == ')');

    auto task = nmf::ParseListItem("* [x] done");
    assert(task && task->isTask && task->taskChecked);
    assert(task->bulletChar == '*');
    assert(task->content == "done");

    auto quote = nmf::ParseListItem("> quoted");
    assert(quote && quote->isQuote);
    assert(quote->quotePrefix == "> ");
    assert(quote->content == "quoted");

    auto nestedQuote = nmf::ParseListItem("> > deep");
    assert(nestedQuote && nestedQuote->quotePrefix == "> > ");

    assert(!nmf::ParseListItem("plain text"));
    assert(!nmf::ParseListItem("-not a list"));
    assert(!nmf::ParseListItem("1.also not"));
    assert(!nmf::ParseListItem(""));
    // "[x]" without trailing space boundary is content, not a task box.
    auto notTask = nmf::ParseListItem("- [x]y");
    assert(notTask && !notTask->isTask);
}

void TestListContinuation() {
    auto bullet = nmf::ContinuationForLine("- item");
    assert(bullet && !bullet->terminate && bullet->prefix == "- ");

    auto nested = nmf::ContinuationForLine("    * sub");
    assert(nested && nested->prefix == "    * ");

    auto ordered = nmf::ContinuationForLine("3. step");
    assert(ordered && ordered->prefix == "4. ");

    auto padded = nmf::ContinuationForLine("09. step");
    assert(padded && padded->prefix == "10. ");
    auto padded2 = nmf::ContinuationForLine("01. step");
    assert(padded2 && padded2->prefix == "02. ");

    auto paren = nmf::ContinuationForLine("1) step");
    assert(paren && paren->prefix == "2) ");

    auto task = nmf::ContinuationForLine("- [x] done");
    assert(task && task->prefix == "- [ ] ");

    auto quote = nmf::ContinuationForLine("> words");
    assert(quote && quote->prefix == "> ");

    auto emptyItem = nmf::ContinuationForLine("- ");
    assert(emptyItem && emptyItem->terminate);
    auto emptyTask = nmf::ContinuationForLine("- [ ] ");
    assert(emptyTask && emptyTask->terminate);
    auto emptyQuote = nmf::ContinuationForLine("> ");
    assert(emptyQuote && emptyQuote->terminate);

    assert(!nmf::ContinuationForLine("plain paragraph"));
    assert(!nmf::ContinuationForLine(""));
}

void TestToggleTaskCheckbox() {
    assert(nmf::ToggleTaskCheckbox("- [ ] todo") == "- [x] todo");
    assert(nmf::ToggleTaskCheckbox("- [x] done") == "- [ ] done");
    assert(nmf::ToggleTaskCheckbox("- [X] DONE") == "- [ ] DONE");
    assert(nmf::ToggleTaskCheckbox("  1. [ ] ordered") == "  1. [x] ordered");
    assert(nmf::ToggleTaskCheckbox("- plain item") == "- [ ] plain item");
    assert(nmf::ToggleTaskCheckbox("  plain text") == "  - [ ] plain text");
    assert(nmf::ToggleTaskCheckbox("") == "- [ ] ");
}

void TestRenumberListBlock() {
    const std::vector<std::string> input{
        "1. one",
        "5. two",
        "2. three",
        "   1. nested a",
        "   7. nested b",
        "9. four",
    };
    const auto result = nmf::RenumberListBlock(input);
    assert(result[0] == "1. one");
    assert(result[1] == "2. two");
    assert(result[2] == "3. three");
    assert(result[3] == "   1. nested a");
    assert(result[4] == "   2. nested b");
    assert(result[5] == "4. four");

    // Starting number is preserved; padding respected.
    const auto offset = nmf::RenumberListBlock({"4. a", "9. b"});
    assert(offset[0] == "4. a");
    assert(offset[1] == "5. b");
    const auto padded = nmf::RenumberListBlock({"01. a", "07. b"});
    assert(padded[0] == "01. a");
    assert(padded[1] == "02. b");

    // Mixed content and tasks survive.
    const auto mixed = nmf::RenumberListBlock({"1. [x] done", "4. [ ] todo", "- bullet", "3. restart"});
    assert(mixed[0] == "1. [x] done");
    assert(mixed[1] == "2. [ ] todo");
    assert(mixed[2] == "- bullet");
    assert(mixed[3] == "3. restart");
}

void TestTableParsingAndFormatting() {
    const std::vector<std::string> lines{
        "| Name | Value |",
        "| :-- | --: |",
        "| a | 1 |",
        "| longer cell | 22 |",
    };
    const auto span = nmf::FindTableBlock(lines, 2);
    assert(span && span->firstLine == 0 && span->lastLine == 3 && span->hasDelimiter);

    const auto table = nmf::ParseTable(lines);
    assert(table);
    assert(table->header.size() == 2);
    assert(table->header[0] == "Name");
    assert(table->alignments[0] == nmf::TableAlignment::Left);
    assert(table->alignments[1] == nmf::TableAlignment::Right);
    assert(table->rows.size() == 2);
    assert(table->rows[1][0] == "longer cell");

    const auto formatted = nmf::FormatTable(*table);
    assert(formatted.size() == 4);
    assert(formatted[0] == "| Name        | Value |");
    assert(formatted[1] == "| :---------- | ----: |");
    assert(formatted[2] == "| a           |     1 |");
    assert(formatted[3] == "| longer cell |    22 |");

    // Escaped pipes stay inside a cell.
    const auto cells = nmf::SplitTableRow("| a \\| b | c |");
    assert(cells.size() == 2);
    assert(cells[0] == "a \\| b");

    // Missing outer pipes still parse.
    const auto loose = nmf::SplitTableRow("one | two");
    assert(loose.size() == 2 && loose[0] == "one" && loose[1] == "two");
}

void TestTableColumnOps() {
    nmf::MarkdownTable table;
    table.header = {"A", "B"};
    table.alignments = {nmf::TableAlignment::None, nmf::TableAlignment::Right};
    table.rows = {{"1", "2"}, {"3", "4"}};

    nmf::InsertTableColumn(table, 0);
    assert(table.header.size() == 3);
    assert(table.header[1].empty());
    assert(table.rows[0].size() == 3 && table.rows[0][2] == "2");
    assert(table.alignments[2] == nmf::TableAlignment::Right);

    assert(nmf::DeleteTableColumn(table, 1));
    assert(table.header.size() == 2);
    assert(table.rows[0][1] == "2");

    nmf::CycleTableColumnAlignment(table, 0);
    assert(table.alignments[0] == nmf::TableAlignment::Left);
    nmf::CycleTableColumnAlignment(table, 0);
    assert(table.alignments[0] == nmf::TableAlignment::Center);
    nmf::CycleTableColumnAlignment(table, 0);
    assert(table.alignments[0] == nmf::TableAlignment::Right);
    nmf::CycleTableColumnAlignment(table, 0);
    assert(table.alignments[0] == nmf::TableAlignment::None);

    nmf::MarkdownTable single;
    single.header = {"only"};
    assert(!nmf::DeleteTableColumn(single, 0));
}

void TestTableCellGeometry() {
    const std::string formatted = "| Name        | Value |";
    const auto ranges = nmf::FormattedCellRanges(formatted);
    assert(ranges.size() == 2);
    assert(formatted.substr(ranges[0].start, ranges[0].end - ranges[0].start) == "Name       ");
    assert(formatted.substr(ranges[1].start, ranges[1].end - ranges[1].start) == "Value");

    assert(nmf::CellIndexForColumn("| a | b | c |", 3) == 0);
    assert(nmf::CellIndexForColumn("| a | b | c |", 6) == 1);
    assert(nmf::CellIndexForColumn("| a | b | c |", 11) == 2);
    assert(nmf::CellIndexForColumn("a | b", 1) == 0);
    assert(nmf::CellIndexForColumn("a | b", 4) == 1);

    assert(nmf::Utf8CodepointCount(u8"héllo") == 5);
    assert(nmf::IsTableLine("| a |"));
    assert(nmf::IsTableLine("a | b"));
    assert(!nmf::IsTableLine("a \\| b"));
    assert(!nmf::IsTableLine("plain"));
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
    TestListItemParsing();
    TestListContinuation();
    TestToggleTaskCheckbox();
    TestRenumberListBlock();
    TestTableParsingAndFormatting();
    TestTableColumnOps();
    TestTableCellGeometry();
    TestWebViewUserDataFolder();
    std::cout << "nmf_tests passed\n";
    return 0;
}
