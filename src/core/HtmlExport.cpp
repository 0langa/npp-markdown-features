#include "core/HtmlExport.h"

#include <array>
#include <cstdio>

namespace nmf {

std::string RemoveSourcepos(std::string html) {
    static const std::string kAttribute = " data-sourcepos=\"";
    size_t position = 0;
    while ((position = html.find(kAttribute, position)) != std::string::npos) {
        const auto closingQuote = html.find('"', position + kAttribute.size());
        if (closingQuote == std::string::npos) {
            break;
        }
        html.erase(position, closingQuote - position + 1);
    }
    return html;
}

std::string BuildClipboardHtml(const std::string& htmlFragment) {
    static const char kHeaderTemplate[] =
        "Version:0.9\r\n"
        "StartHTML:%010u\r\n"
        "EndHTML:%010u\r\n"
        "StartFragment:%010u\r\n"
        "EndFragment:%010u\r\n";
    static const std::string kPrefix = "<html><body>\r\n<!--StartFragment-->";
    static const std::string kSuffix = "<!--EndFragment-->\r\n</body></html>";

    // Header length is constant thanks to the fixed-width numbers.
    std::array<char, 256> probe{};
    const int headerLength = std::snprintf(probe.data(), probe.size(), kHeaderTemplate, 0u, 0u, 0u, 0u);

    const unsigned startHtml = static_cast<unsigned>(headerLength);
    const unsigned startFragment = static_cast<unsigned>(headerLength + kPrefix.size());
    const unsigned endFragment = static_cast<unsigned>(startFragment + htmlFragment.size());
    const unsigned endHtml = static_cast<unsigned>(endFragment + kSuffix.size());

    std::array<char, 256> header{};
    std::snprintf(header.data(), header.size(), kHeaderTemplate, startHtml, endHtml, startFragment, endFragment);

    std::string payload(header.data());
    payload += kPrefix;
    payload += htmlFragment;
    payload += kSuffix;
    return payload;
}

}  // namespace nmf
