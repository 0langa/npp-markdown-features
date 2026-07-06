#pragma once

#include <string>
#include <vector>

namespace nmf {

// Should toggling `marker` around a range unwrap existing markers instead of
// wrapping? `before` is the text immediately preceding the range (last few
// chars), `after` the text immediately following it.
bool ShouldUnwrapMarker(const std::string& before, const std::string& after, const std::string& marker);

// Change an ATX heading level by delta (clamped 0..6). Level 0 = plain text.
// "# Title" +1 -> "## Title"; "# Title" -1 -> "Title"; "Text" +1 -> "# Text".
std::string ChangeHeadingLevel(const std::string& line, int delta);

// Add one "> " level to all lines, or remove one level when every non-blank
// line is already quoted. Blank lines get a bare ">" when quoting.
std::vector<std::string> ToggleBlockquoteLines(const std::vector<std::string>& lines);

}  // namespace nmf
