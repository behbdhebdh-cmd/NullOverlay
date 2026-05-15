#pragma once

#include "pch.h"

namespace render {
struct RenderContext;
}

namespace modules {

class Module {
public:
    explicit Module(std::string name, bool enabledByDefault = true)
        : name_(std::move(name)),
          enabled_(enabledByDefault) {
    }

    virtual ~Module() = default;

    const std::string& name() const { return name_; }
    bool isEnabled() const { return enabled_; }
    bool isActive() const { return enabled_ && !safetySuspended_; }

    void setEnabled(bool enabled) {
        if (enabled_ == enabled) {
            return;
        }
        enabled_ = enabled;
        if (!safetySuspended_) {
            enabled_ ? onEnable() : onDisable();
        }
    }

    void setSafetySuspended(bool suspended) {
        if (safetySuspended_ == suspended) {
            return;
        }
        safetySuspended_ = suspended;
        if (enabled_) {
            safetySuspended_ ? onDisable() : onEnable();
        }
    }

    void render(const render::RenderContext& context) {
        if (isActive()) {
            onRender(context);
        }
    }

    virtual void onEnable() {}
    virtual void onDisable() {}
    virtual void onRender(const render::RenderContext& context) = 0;

private:
    std::string name_;
    bool enabled_{};
    bool safetySuspended_{};
};

} // namespace modules
