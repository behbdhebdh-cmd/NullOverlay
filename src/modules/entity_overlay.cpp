#include "modules/entity_overlay.h"

#include "core/safety_guard.h"
#include "render/menu.h"
#include "render/overlay_renderer.h"
#include "sdk/minecraft.h"

namespace modules {

EntityOverlay::EntityOverlay()
    : Module("Entity Debug Visualizer", true) {
}

void EntityOverlay::onEnable() {
    clearCache();
    lastRefresh_ = {};
}

void EntityOverlay::onDisable() {
    clearCache();
}

void EntityOverlay::onRender(const render::RenderContext& context) {
    if (!render::Menu::instance().settings().entityVisualizerEnabled) {
        clearCache();
        return;
    }

    if (!core::SafetyGuard::instance().isSingleplayerSafe()) {
        clearCache();
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (lastRefresh_.time_since_epoch().count() == 0 ||
        now - lastRefresh_ > std::chrono::milliseconds(150)) {
        refreshCache();
        lastRefresh_ = now;
    }

    std::vector<sdk::EntitySnapshot> entities;
    {
        std::lock_guard lock(mutex_);
        entities = cachedEntities_;
    }

    render::OverlayRenderer::instance().renderEntities(
        entities,
        context,
        render::Menu::instance().settings());
}

void EntityOverlay::refreshCache() {
    std::vector<sdk::EntitySnapshot> next;
    const auto filters = render::Menu::instance().settings().filters;
    if (!sdk::Minecraft::instance().collectEntities(filters, next)) {
        clearCache();
        return;
    }

    std::lock_guard lock(mutex_);
    cachedEntities_ = std::move(next);
}

void EntityOverlay::clearCache() {
    std::lock_guard lock(mutex_);
    cachedEntities_.clear();
}

} // namespace modules
