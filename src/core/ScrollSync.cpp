#include "core/ScrollSync.h"

#include <algorithm>
#include <cmath>

namespace nmf {

ScrollTarget BlockScrollSyncStrategy::RawToRendered(const MarkdownOutline& outline, int firstVisibleDocLine, double ratio) const {
    ScrollTarget target{std::clamp(ratio, 0.0, 1.0), {}, -1.0};
    if (firstVisibleDocLine >= 0) {
        target.sourceLine = static_cast<double>(firstVisibleDocLine) + 1.0;
    }
    if (const auto heading = outline.HeadingAtOrBeforeLine(firstVisibleDocLine)) {
        target.anchorId = heading->anchorId;
    }
    return target;
}

int BlockScrollSyncStrategy::RenderedToRawLine(const ScrollTarget& captured, const MarkdownOutline& outline) const {
    if (captured.sourceLine >= 1.0) {
        return static_cast<int>(std::floor(captured.sourceLine)) - 1;
    }
    if (!captured.anchorId.empty()) {
        if (const auto heading = outline.HeadingByAnchor(captured.anchorId)) {
            return heading->line;
        }
    }
    return -1;
}

}  // namespace nmf
