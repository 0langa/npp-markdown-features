#pragma once

#include <string>

namespace nmf {

// Remove data-sourcepos attributes from rendered HTML — they are an
// internal scroll-sync detail and just noise in exported documents.
std::string RemoveSourcepos(std::string html);

// Build a CF_HTML ("HTML Format") clipboard payload around a UTF-8 HTML
// fragment. Offsets are byte positions as the spec requires.
std::string BuildClipboardHtml(const std::string& htmlFragment);

}  // namespace nmf
