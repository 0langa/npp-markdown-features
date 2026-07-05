#pragma once

#include "core/Feature.h"

#include <memory>
#include <vector>

namespace nmf {

class FeatureRegistry {
public:
    void Add(std::unique_ptr<Feature> feature);
    void LoadSettings(const AppSettings& settings);
    void SaveSettings(AppSettings& settings) const;
    void DispatchCommand(PluginCommand command, const ActiveDocument& document);
    void NotifyDocumentChanged(const ActiveDocument& document);
    void NotifyFileSaved(const ActiveDocument& document);

private:
    std::vector<std::unique_ptr<Feature>> features_;
};

}  // namespace nmf
