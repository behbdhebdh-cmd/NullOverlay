#pragma once

#include "pch.h"
#include "modules/module.h"

namespace modules {

class ModuleManager {
public:
    static ModuleManager& instance();

    void initializeDefaults();
    void shutdown();
    void renderAll(const render::RenderContext& context);
    void suspendForSafety();
    void resumeAfterSafetyClear();
    void disableAll();

    const std::vector<std::unique_ptr<Module>>& modules() const { return modules_; }

private:
    ModuleManager() = default;

    std::mutex mutex_;
    bool initialized_{};
    bool safetySuspended_{};
    std::vector<std::unique_ptr<Module>> modules_;
};

} // namespace modules
