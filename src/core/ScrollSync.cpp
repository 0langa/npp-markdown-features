#include "core/ScrollSync.h"

namespace nmf {

ScrollTarget SectionScrollSyncStrategy::RawToRendered(const MarkdownOutline& outline, int firstVisibleLine, double ratio) const {
    ScrollTarget target{ratio, {}};
    if (const auto heading = outline.HeadingAtOrBeforeLine(firstVisibleLine)) {
        target.anchorId = heading->anchorId;
    }
    return target;
}

ScrollTarget SectionScrollSyncStrategy::RenderedToRaw(const MarkdownOutline& outline, const std::string& anchorId, double ratio) const {
    ScrollTarget target{ratio, {}};
    if (const auto heading = outline.HeadingByAnchor(anchorId)) {
        target.anchorId = heading->anchorId;
    }
    return target;
}

}  // namespace nmf
