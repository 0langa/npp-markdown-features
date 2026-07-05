#pragma once

#include "core/MarkdownOutline.h"

#include <optional>
#include <string>

namespace nmf {

struct ScrollTarget {
    double ratio{0.0};
    std::string anchorId;
};

class ScrollSyncStrategy {
public:
    virtual ~ScrollSyncStrategy() = default;
    virtual ScrollTarget RawToRendered(const MarkdownOutline& outline, int firstVisibleLine, double ratio) const = 0;
    virtual ScrollTarget RenderedToRaw(const MarkdownOutline& outline, const std::string& anchorId, double ratio) const = 0;
};

class SectionScrollSyncStrategy final : public ScrollSyncStrategy {
public:
    ScrollTarget RawToRendered(const MarkdownOutline& outline, int firstVisibleLine, double ratio) const override;
    ScrollTarget RenderedToRaw(const MarkdownOutline& outline, const std::string& anchorId, double ratio) const override;
};

}  // namespace nmf
