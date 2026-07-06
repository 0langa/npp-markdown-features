#pragma once

#include "core/MarkdownOutline.h"

#include <optional>
#include <string>

namespace nmf {

// sourceLine is a 1-based, fractional Markdown source line matching cmark's
// data-sourcepos convention; values below 1 mean "unknown, use fallbacks".
struct ScrollTarget {
    double ratio{0.0};
    std::string anchorId;
    double sourceLine{-1.0};
};

class ScrollSyncStrategy {
public:
    virtual ~ScrollSyncStrategy() = default;
    virtual ScrollTarget RawToRendered(const MarkdownOutline& outline, int firstVisibleDocLine, double ratio) const = 0;
    virtual int RenderedToRawLine(const ScrollTarget& captured, const MarkdownOutline& outline) const = 0;
};

// Block-level sync: primary channel is the fractional source line resolved
// against data-sourcepos blocks in the rendered DOM; heading anchor and
// viewport ratio remain as ordered fallbacks.
class BlockScrollSyncStrategy final : public ScrollSyncStrategy {
public:
    ScrollTarget RawToRendered(const MarkdownOutline& outline, int firstVisibleDocLine, double ratio) const override;
    int RenderedToRawLine(const ScrollTarget& captured, const MarkdownOutline& outline) const override;
};

}  // namespace nmf
