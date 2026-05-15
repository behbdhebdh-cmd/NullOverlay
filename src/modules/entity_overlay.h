#pragma once

#include "pch.h"
#include "modules/module.h"
#include "sdk/entity.h"

namespace modules {

class EntityOverlay final : public Module {
public:
    EntityOverlay();

    void onEnable() override;
    void onDisable() override;
    void onRender(const render::RenderContext& context) override;

private:
    void refreshCache();
    void clearCache();

    std::mutex mutex_;
    std::vector<sdk::EntitySnapshot> cachedEntities_;
    std::chrono::steady_clock::time_point lastRefresh_{};
};

} // namespace modules
