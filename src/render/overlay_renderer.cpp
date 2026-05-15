#include "render/overlay_renderer.h"

#include "imgui.h"

namespace {

ImVec4 colorVecForEntity(const sdk::EntitySnapshot& entity, const render::OverlaySettings& settings) {
    switch (entity.category) {
    case sdk::EntityCategory::Player:
        return settings.playerColor;
    case sdk::EntityCategory::Mob:
        return settings.mobColor;
    case sdk::EntityCategory::Animal:
        return settings.animalColor;
    case sdk::EntityCategory::Item:
        return settings.itemColor;
    case sdk::EntityCategory::Unknown:
        return settings.mobColor;
    }
    return settings.textColor;
}

ImU32 colorForEntity(const sdk::EntitySnapshot& entity, const render::OverlaySettings& settings, float alpha = 1.0f) {
    ImVec4 color = colorVecForEntity(entity, settings);
    color.w *= std::clamp(alpha, 0.0f, 1.0f);
    return ImGui::GetColorU32(color);
}

std::string cleanedName(std::string_view name, sdk::EntityCategory fallbackCategory) {
    std::string cleaned;
    cleaned.reserve(name.size());

    for (std::size_t i = 0; i < name.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(name[i]);
        if (ch == 0xC2 && i + 1 < name.size() && static_cast<unsigned char>(name[i + 1]) == 0xA7) {
            ++i;
            if (i + 1 < name.size()) {
                ++i;
            }
            continue;
        }
        if (name[i] == '\xA7') {
            if (i + 1 < name.size()) {
                ++i;
            }
            continue;
        }
        if (ch >= 32 && ch != 127) {
            cleaned.push_back(static_cast<char>(ch));
        }
    }

    while (!cleaned.empty() && std::isspace(static_cast<unsigned char>(cleaned.front()))) {
        cleaned.erase(cleaned.begin());
    }
    while (!cleaned.empty() && std::isspace(static_cast<unsigned char>(cleaned.back()))) {
        cleaned.pop_back();
    }

    if (cleaned.empty()) {
        cleaned = sdk::categoryName(fallbackCategory);
    }
    return cleaned;
}

std::string labelForEntity(const sdk::EntitySnapshot& entity, const render::OverlaySettings& settings) {
    std::ostringstream label;

    bool needsSeparator = false;
    if (settings.drawNames) {
        label << cleanedName(entity.name, entity.category);
        needsSeparator = true;
    }

    if (settings.drawCategoryLabels) {
        if (needsSeparator) {
            label << " ";
        }
        label << "[" << sdk::categoryName(entity.category) << "]";
        needsSeparator = true;
    }

    if (settings.drawHealth && entity.health >= 0.0f) {
        if (needsSeparator) {
            label << " ";
        }
        label << std::fixed << std::setprecision(0) << entity.health << "hp";
        needsSeparator = true;
    }

    if (settings.drawDistance) {
        if (needsSeparator) {
            label << " ";
        }
        label << std::fixed << std::setprecision(0) << entity.distance << "m";
    }

    return label.str();
}

struct LabelCommand {
    std::string text;
    ImVec2 pos{};
    ImVec2 size{};
    ImU32 textColor{};
    ImU32 shadowColor{};
    ImU32 accentColor{};
    ImU32 backgroundColor{};
    float fontSize{};
};

bool intersects(const ImVec4& a, const ImVec4& b) {
    return a.x < b.z && a.z > b.x && a.y < b.w && a.w > b.y;
}

ImVec4 labelRect(const ImVec2& pos, const ImVec2& size) {
    return ImVec4(pos.x - 6.0f, pos.y - 4.0f, pos.x + size.x + 8.0f, pos.y + size.y + 5.0f);
}

void drawCornerBox(ImDrawList* drawList,
                   const ImVec2& min,
                   const ImVec2& max,
                   ImU32 color,
                   ImU32 shadowColor,
                   float thickness) {
    const float width = max.x - min.x;
    const float height = max.y - min.y;
    const float corner = std::clamp(std::min(width, height) * 0.28f, 5.0f, 18.0f);
    const float shadowThickness = thickness + 2.0f;

    auto drawCorners = [&](ImU32 drawColor, float lineThickness, float offset) {
        const ImVec2 a(min.x + offset, min.y + offset);
        const ImVec2 b(max.x + offset, max.y + offset);
        drawList->AddLine(a, ImVec2(a.x + corner, a.y), drawColor, lineThickness);
        drawList->AddLine(a, ImVec2(a.x, a.y + corner), drawColor, lineThickness);
        drawList->AddLine(ImVec2(b.x, a.y), ImVec2(b.x - corner, a.y), drawColor, lineThickness);
        drawList->AddLine(ImVec2(b.x, a.y), ImVec2(b.x, a.y + corner), drawColor, lineThickness);
        drawList->AddLine(ImVec2(a.x, b.y), ImVec2(a.x + corner, b.y), drawColor, lineThickness);
        drawList->AddLine(ImVec2(a.x, b.y), ImVec2(a.x, b.y - corner), drawColor, lineThickness);
        drawList->AddLine(b, ImVec2(b.x - corner, b.y), drawColor, lineThickness);
        drawList->AddLine(b, ImVec2(b.x, b.y - corner), drawColor, lineThickness);
    };

    drawCorners(shadowColor, shadowThickness, 1.0f);
    drawList->AddRectFilled(min, max, IM_COL32(0, 0, 0, 22), 3.0f);
    drawCorners(color, thickness, 0.0f);
}

} // namespace

namespace render {

OverlayRenderer& OverlayRenderer::instance() {
    static OverlayRenderer renderer;
    return renderer;
}

void OverlayRenderer::renderEntities(const std::vector<sdk::EntitySnapshot>& entities,
                                     const RenderContext& context,
                                     const OverlaySettings& settings) {
    if (!context.camera.valid || context.width <= 0 || context.height <= 0) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImU32 textColor = ImGui::GetColorU32(settings.textColor);
    const ImU32 shadowColor = IM_COL32(0, 0, 0, 210);
    const ImU32 labelBackground = IM_COL32(6, 10, 16, 190);
    const float fontSize = ImGui::GetFontSize() * std::clamp(settings.labelScale, 0.75f, 1.35f);
    ImFont* font = ImGui::GetFont();
    std::vector<LabelCommand> labels;
    std::vector<ImVec4> occupiedLabels;

    for (const auto& entity : entities) {
        util::Vec2 feet{};
        util::Vec2 head{};
        util::Vec3 headPosition = entity.position;
        headPosition.y += static_cast<double>(entity.height);

        const bool feetVisible = util::worldToScreen(entity.position, context.camera, context.width, context.height, feet);
        const bool headVisible = util::worldToScreen(headPosition, context.camera, context.width, context.height, head);
        if (!feetVisible && !headVisible) {
            continue;
        }

        const ImU32 entityColor = colorForEntity(entity, settings);
        const ImU32 translucentEntityColor = colorForEntity(entity, settings, 0.35f);
        const float x = feetVisible && headVisible ? (feet.x + head.x) * 0.5f : (feetVisible ? feet.x : head.x);
        const float y = feetVisible ? feet.y : head.y;

        if (settings.drawSnaplines && feetVisible) {
            const ImVec2 start(static_cast<float>(context.width) * 0.5f, static_cast<float>(context.height) - 4.0f);
            const ImVec2 end(x, y);
            drawList->AddLine(ImVec2(start.x + 1.0f, start.y + 1.0f), ImVec2(end.x + 1.0f, end.y + 1.0f), shadowColor, 2.0f);
            drawList->AddLine(start, end, colorForEntity(entity, settings, 0.42f), 1.0f);
        }

        if (settings.drawMarkers) {
            drawList->AddCircleFilled(ImVec2(x, y), settings.markerSize + 2.5f, shadowColor, 18);
            drawList->AddCircleFilled(ImVec2(x, y), settings.markerSize, translucentEntityColor, 18);
            drawList->AddCircle(ImVec2(x, y), settings.markerSize + 1.0f, entityColor, 18, 1.5f);
        }

        if (settings.drawBoxes && feetVisible && headVisible) {
            const float topY = std::min(feet.y, head.y);
            const float bottomY = std::max(feet.y, head.y);
            const float boxHeight = std::max(14.0f, bottomY - topY);
            const float boxWidth = std::clamp(boxHeight * 0.42f, 8.0f, 160.0f);
            const float boxCenterX = (feet.x + head.x) * 0.5f;
            const ImVec2 min(boxCenterX - boxWidth * 0.5f, topY);
            const ImVec2 max(boxCenterX + boxWidth * 0.5f, bottomY);
            drawCornerBox(drawList, min, max, entityColor, shadowColor, settings.lineThickness);

            if (settings.drawHealthBars && entity.health >= 0.0f) {
                const float ratio = std::clamp(entity.health / 20.0f, 0.0f, 1.0f);
                const float barX = min.x - 6.0f;
                const float fillTop = max.y - boxHeight * ratio;
                drawList->AddRectFilled(ImVec2(barX, min.y), ImVec2(barX + 3.0f, max.y), IM_COL32(0, 0, 0, 185), 2.0f);
                const ImU32 healthColor = ImGui::GetColorU32(ImVec4(1.0f - ratio, ratio, 0.14f, 1.0f));
                drawList->AddRectFilled(ImVec2(barX, fillTop), ImVec2(barX + 3.0f, max.y), healthColor, 2.0f);
            }
        }

        const std::string label = labelForEntity(entity, settings);
        if (!label.empty()) {
            const ImVec2 textSize = font->CalcTextSizeA(fontSize, 10000.0f, 0.0f, label.c_str());
            const float unclampedX = x - textSize.x * 0.5f;
            const float unclampedY = (headVisible ? std::min(head.y, y) : y) - textSize.y - 5.0f;
            const float textX = std::clamp(unclampedX, 2.0f, std::max(2.0f, static_cast<float>(context.width) - textSize.x - 2.0f));
            float textY = std::clamp(unclampedY, 2.0f, std::max(2.0f, static_cast<float>(context.height) - textSize.y - 2.0f));
            ImVec4 rect{};
            for (int attempt = 0; attempt < 12; ++attempt) {
                rect = labelRect(ImVec2(textX, textY), textSize);
                bool overlaps = false;
                for (const auto& occupied : occupiedLabels) {
                    if (intersects(rect, occupied)) {
                        overlaps = true;
                        break;
                    }
                }
                if (!overlaps) {
                    break;
                }
                textY = std::max(2.0f, textY - textSize.y - 7.0f);
            }

            occupiedLabels.push_back(labelRect(ImVec2(textX, textY), textSize));
            labels.push_back(LabelCommand{
                label,
                ImVec2(textX, textY),
                textSize,
                textColor,
                shadowColor,
                entityColor,
                labelBackground,
                fontSize,
            });
        }
    }

    if (settings.drawLabelBackgrounds) {
        for (const auto& label : labels) {
            const ImVec4 rect = labelRect(label.pos, label.size);
            drawList->AddRectFilled(ImVec2(rect.x, rect.y), ImVec2(rect.z, rect.w), label.backgroundColor, 4.0f);
            drawList->AddRectFilled(ImVec2(rect.x, rect.y), ImVec2(rect.x + 3.0f, rect.w), label.accentColor, 4.0f, ImDrawFlags_RoundCornersLeft);
        }
    }

    for (const auto& label : labels) {
        drawList->AddText(font, label.fontSize, ImVec2(label.pos.x + 1.0f, label.pos.y + 1.0f), label.shadowColor, label.text.c_str());
        drawList->AddText(font, label.fontSize, label.pos, label.textColor, label.text.c_str());
    }
}

} // namespace render
