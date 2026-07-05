#include "core/FeatureRegistry.h"

namespace nmf {

void FeatureRegistry::Add(std::unique_ptr<Feature> feature) {
    features_.push_back(std::move(feature));
}

void FeatureRegistry::LoadSettings(const AppSettings& settings) {
    for (auto& feature : features_) {
        feature->LoadSettings(settings);
    }
}

void FeatureRegistry::SaveSettings(AppSettings& settings) const {
    for (const auto& feature : features_) {
        feature->SaveSettings(settings);
    }
}

void FeatureRegistry::DispatchCommand(PluginCommand command, const ActiveDocument& document) {
    for (auto& feature : features_) {
        feature->OnCommand(command, document);
    }
}

void FeatureRegistry::NotifyDocumentChanged(const ActiveDocument& document) {
    for (auto& feature : features_) {
        feature->OnDocumentChanged(document);
    }
}

void FeatureRegistry::NotifyFileSaved(const ActiveDocument& document) {
    for (auto& feature : features_) {
        feature->OnFileSaved(document);
    }
}

}  // namespace nmf
