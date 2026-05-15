#pragma once

#include "pch.h"
#include "sdk/entity.h"

#include "imgui.h"

namespace render {

struct OverlaySettings {
    bool entityVisualizerEnabled{false};
    bool drawNames{true};
    bool drawHealth{true};
    bool drawDistance{true};
    bool drawBoxes{true};
    bool drawMarkers{false};
    bool drawCategoryLabels{false};
    bool drawHealthBars{true};
    bool drawSnaplines{false};
    bool drawLabelBackgrounds{true};
    bool fullbrightEnabled{false};
    bool fpsCounterEnabled{true};
    float markerSize{5.0f};
    float lineThickness{1.5f};
    float labelScale{1.0f};
    sdk::EntityFilters filters{};
    ImVec4 playerColor{0.35f, 0.62f, 1.0f, 1.0f};
    ImVec4 mobColor{1.0f, 0.36f, 0.32f, 1.0f};
    ImVec4 animalColor{0.34f, 0.86f, 0.54f, 1.0f};
    ImVec4 itemColor{1.0f, 0.78f, 0.28f, 1.0f};
    ImVec4 textColor{0.92f, 0.94f, 0.98f, 1.0f};
};

class Menu {
public:
    static Menu& instance();

    void applyStyle();
    void render(std::string_view safetyStatus);
    void renderHud();
    void toggleVisible() { visible_ = !visible_; }
    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }

    OverlaySettings& settings() { return settings_; }
    const OverlaySettings& settings() const { return settings_; }

private:
    Menu() = default;

    bool visible_{true};
    int activeTab_{};
    OverlaySettings settings_{};
};

} // namespace render
