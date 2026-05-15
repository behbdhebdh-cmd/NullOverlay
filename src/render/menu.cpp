#include "render/menu.h"

#include "core/hooks.h"
#include "util/logging.h"

namespace {

bool toggleSwitch(const char* label, bool* value) {
    ImGui::PushID(label);

    const float height = ImGui::GetFrameHeight();
    const float width = height * 1.8f;
    const float radius = height * 0.5f;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(250.0f);

    bool changed = false;
    if (ImGui::InvisibleButton("toggle", ImVec2(width, height))) {
        *value = !*value;
        changed = true;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float t = *value ? 1.0f : 0.0f;
    const ImU32 background = ImGui::GetColorU32(*value ? ImVec4(0.25f, 0.58f, 0.95f, 1.0f) : ImVec4(0.20f, 0.22f, 0.26f, 1.0f));
    drawList->AddRectFilled(min, max, background, radius);
    drawList->AddCircleFilled(
        ImVec2(min.x + radius + t * (width - height), min.y + radius),
        radius - 3.0f,
        ImGui::GetColorU32(ImVec4(0.96f, 0.97f, 0.99f, 1.0f)));

    ImGui::PopID();
    return changed;
}

void colorEditor(const char* label, ImVec4& color) {
    ImGui::ColorEdit4(label, &color.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
}

} // namespace

namespace render {

Menu& Menu::instance() {
    static Menu menu;
    return menu;
}

void Menu::applyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.Alpha = 1.0f;
    style.WindowRounding = 7.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 4.0f;
    style.WindowPadding = ImVec2(16.0f, 13.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(11.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 5.0f);
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.68f, 0.72f, 0.78f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.025f, 0.030f, 0.040f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.050f, 0.060f, 0.078f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.035f, 0.042f, 0.055f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.36f, 0.50f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.21f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.23f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.09f, 0.11f, 0.14f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.36f, 0.65f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.36f, 0.65f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.48f, 0.76f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.16f, 0.18f, 0.23f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.23f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.31f, 0.38f, 0.48f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.17f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.23f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.31f, 0.38f, 0.48f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.20f, 0.27f, 0.35f, 1.00f);
}

void Menu::render(std::string_view safetyStatus) {
    if (!visible_) {
        static bool loggedHidden = false;
        if (!loggedHidden) {
            util::logInfo("Menu::render skipped because menu is hidden");
            loggedHidden = true;
        }
        return;
    }

    static std::uint64_t renderCalls = 0;
    ++renderCalls;
    if (renderCalls <= 30 || renderCalls % 300 == 0) {
        std::ostringstream message;
        message << "Menu::render call #" << renderCalls
                << ", safetyStatus=\"" << safetyStatus << "\""
                << ", io.DisplaySize=" << ImGui::GetIO().DisplaySize.x << "x" << ImGui::GetIO().DisplaySize.y;
        util::logInfo(message.str());
    }

    constexpr float kHeaderHeight = 42.0f;
    constexpr float kPanelRounding = 8.0f;

    ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(690.0f, 520.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(1.0f);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings;

    if (!ImGui::Begin("NULL CLIENT // Forge 1.21.1", &visible_, flags)) {
        ImGui::End();
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    if (renderCalls <= 30 || renderCalls % 300 == 0) {
        std::ostringstream message;
        message << "Menu ImGui window active: pos=" << windowPos.x << "," << windowPos.y
                << ", size=" << windowSize.x << "x" << windowSize.y
                << ", focused=" << (ImGui::IsWindowFocused() ? "true" : "false")
                << ", hovered=" << (ImGui::IsWindowHovered() ? "true" : "false");
        util::logInfo(message.str());
    }
    const ImVec2 windowMax(windowPos.x + windowSize.x, windowPos.y + windowSize.y);
    drawList->AddRectFilled(windowPos, windowMax, IM_COL32(6, 9, 15, 252), kPanelRounding);
    drawList->AddRectFilled(windowPos, ImVec2(windowMax.x, windowPos.y + kHeaderHeight), IM_COL32(16, 23, 34, 255), kPanelRounding, ImDrawFlags_RoundCornersTop);
    drawList->AddRectFilled(ImVec2(windowPos.x, windowPos.y + kHeaderHeight - 2.0f), ImVec2(windowMax.x, windowPos.y + kHeaderHeight), IM_COL32(69, 156, 255, 210));
    drawList->AddRect(windowPos, windowMax, IM_COL32(68, 112, 168, 255), kPanelRounding, 0, 1.5f);

    ImGui::SetCursorPos(ImVec2(16.0f, 12.0f));
    ImGui::TextUnformatted("NULL CLIENT // Forge 1.21.1");
    ImGui::SameLine(ImGui::GetWindowWidth() - 38.0f);
    if (ImGui::Button("X", ImVec2(24.0f, 22.0f))) {
        visible_ = false;
    }

    ImGui::SetCursorPos(ImVec2(16.0f, kHeaderHeight + 12.0f));
    ImGui::Text("State: %.*s", static_cast<int>(safetyStatus.size()), safetyStatus.data());
    ImGui::Separator();

    if (ImGui::BeginTabBar("null-overlay-tabs")) {
        if (ImGui::BeginTabItem("ESP")) {
            toggleSwitch("Draw names", &settings_.drawNames);
            toggleSwitch("Label backgrounds", &settings_.drawLabelBackgrounds);
            toggleSwitch("Draw health", &settings_.drawHealth);
            toggleSwitch("Health bars", &settings_.drawHealthBars);
            toggleSwitch("Draw distance", &settings_.drawDistance);
            toggleSwitch("Draw boxes", &settings_.drawBoxes);
            toggleSwitch("Draw markers", &settings_.drawMarkers);
            toggleSwitch("Snaplines", &settings_.drawSnaplines);
            toggleSwitch("Category labels", &settings_.drawCategoryLabels);
            ImGui::SliderFloat("Label scale", &settings_.labelScale, 0.75f, 1.35f, "%.2f");
            ImGui::SliderFloat("Marker size", &settings_.markerSize, 2.0f, 12.0f, "%.1f");
            ImGui::SliderFloat("Line thickness", &settings_.lineThickness, 1.0f, 4.0f, "%.1f");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Visualizer")) {
            ImGui::Checkbox("Entity visualizer", &settings_.entityVisualizerEnabled);
            ImGui::SliderFloat("Max distance", &settings_.filters.maxDistance, 8.0f, 256.0f, "%.0f blocks");
            ImGui::Separator();
            ImGui::Checkbox("Players", &settings_.filters.players);
            ImGui::Checkbox("Mobs", &settings_.filters.mobs);
            ImGui::Checkbox("Animals", &settings_.filters.animals);
            ImGui::Checkbox("Items", &settings_.filters.items);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Rendering")) {
            colorEditor("Players", settings_.playerColor);
            colorEditor("Mobs", settings_.mobColor);
            colorEditor("Animals", settings_.animalColor);
            colorEditor("Items", settings_.itemColor);
            colorEditor("Text", settings_.textColor);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Settings")) {
            toggleSwitch("Fullbright", &settings_.fullbrightEnabled);
            toggleSwitch("FPS counter", &settings_.fpsCounterEnabled);
            ImGui::Separator();
            if (ImGui::Button("Unload now")) {
                core::Hooks::requestUnload();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Debug")) {
            ImGui::Text("Version: %s", NULL_OVERLAY_VERSION);
            const float framerate = ImGui::GetIO().Framerate;
            ImGui::Text("Frame time: %.3f ms", framerate > 0.0f ? 1000.0f / framerate : 0.0f);
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void Menu::renderHud() {
    if (!settings_.fpsCounterEnabled) {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const float fps = io.Framerate;
    const float frameMs = fps > 0.0f ? 1000.0f / fps : 0.0f;

    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%.0f FPS  %.1f ms", fps, frameMs);

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImVec2 textSize = ImGui::CalcTextSize(buffer);
    const ImVec2 padding(10.0f, 6.0f);
    const ImVec2 display = io.DisplaySize;
    const ImVec2 min(std::max(8.0f, display.x - textSize.x - padding.x * 2.0f - 14.0f), 14.0f);
    const ImVec2 max(min.x + textSize.x + padding.x * 2.0f, min.y + textSize.y + padding.y * 2.0f);

    drawList->AddRectFilled(ImVec2(min.x + 1.0f, min.y + 1.0f), ImVec2(max.x + 1.0f, max.y + 1.0f), IM_COL32(0, 0, 0, 140), 5.0f);
    drawList->AddRectFilled(min, max, IM_COL32(8, 13, 22, 210), 5.0f);
    drawList->AddRect(min, max, IM_COL32(86, 166, 255, 220), 5.0f, 0, 1.0f);
    drawList->AddText(ImVec2(min.x + padding.x, min.y + padding.y), IM_COL32(235, 243, 255, 255), buffer);
}

} // namespace render
