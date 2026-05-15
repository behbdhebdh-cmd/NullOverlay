#include "modules/module_manager.h"

#include "core/safety_guard.h"
#include "modules/entity_overlay.h"
#include "util/logging.h"

namespace modules {

ModuleManager& ModuleManager::instance() {
    static ModuleManager manager;
    return manager;
}

void ModuleManager::initializeDefaults() {
    std::lock_guard lock(mutex_);
    if (initialized_) {
        return;
    }

    modules_.push_back(std::make_unique<EntityOverlay>());
    for (const auto& module : modules_) {
        if (module->isEnabled()) {
            module->onEnable();
        }
    }
    initialized_ = true;
    util::logInfo("ModuleManager initialized");
}

void ModuleManager::shutdown() {
    std::lock_guard lock(mutex_);
    for (const auto& module : modules_) {
        module->setEnabled(false);
    }
    modules_.clear();
    initialized_ = false;
    safetySuspended_ = false;
}

void ModuleManager::renderAll(const render::RenderContext& context) {
    if (core::SafetyGuard::instance().shouldDisableOverlay()) {
        suspendForSafety();
        return;
    }

    resumeAfterSafetyClear();

    std::lock_guard lock(mutex_);
    for (const auto& module : modules_) {
        module->render(context);
    }
}

void ModuleManager::suspendForSafety() {
    std::lock_guard lock(mutex_);
    if (safetySuspended_) {
        return;
    }

    for (const auto& module : modules_) {
        module->setSafetySuspended(true);
    }
    safetySuspended_ = true;
}

void ModuleManager::resumeAfterSafetyClear() {
    std::lock_guard lock(mutex_);
    if (!safetySuspended_) {
        return;
    }

    for (const auto& module : modules_) {
        module->setSafetySuspended(false);
    }
    safetySuspended_ = false;
}

void ModuleManager::disableAll() {
    std::lock_guard lock(mutex_);
    for (const auto& module : modules_) {
        module->setEnabled(false);
    }
}

} // namespace modules
