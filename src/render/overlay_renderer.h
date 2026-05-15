#pragma once

#include "pch.h"
#include "render/menu.h"
#include "sdk/entity.h"
#include "util/math.h"

namespace render {

struct RenderContext {
    int width{};
    int height{};
    util::CameraState camera{};
};

class OverlayRenderer {
public:
    static OverlayRenderer& instance();

    void renderEntities(const std::vector<sdk::EntitySnapshot>& entities,
                        const RenderContext& context,
                        const OverlaySettings& settings);

private:
    OverlayRenderer() = default;
};

} // namespace render
