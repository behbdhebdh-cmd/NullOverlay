#include "core/safety_guard.h"

#include "core/jvm_wrapper.h"
#include "modules/module_manager.h"
#include "sdk/minecraft.h"
#include "util/logging.h"

namespace {

constexpr auto kSafetyPollInterval = std::chrono::milliseconds(500);

const char* stateName(core::SafetyState state) {
    switch (state) {
    case core::SafetyState::Unknown:
        return "unknown";
    case core::SafetyState::SafeSingleplayer:
        return "safe-singleplayer";
    case core::SafetyState::Unsafe:
        return "unsafe";
    }
    return "unknown";
}

} // namespace

namespace core {

SafetyGuard& SafetyGuard::instance() {
    static SafetyGuard guard;
    return guard;
}

void SafetyGuard::initialize() {
    std::lock_guard lock(mutex_);
    state_ = SafetyState::Unknown;
    reason_ = "Not yet verified";
    lastCheck_ = {};
}

void SafetyGuard::shutdown() {
    std::lock_guard lock(mutex_);
    state_ = SafetyState::Unsafe;
    reason_ = "Overlay is shutting down";
}

void SafetyGuard::update(bool force) {
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard lock(mutex_);
        if (!force && lastCheck_.time_since_epoch().count() != 0 && now - lastCheck_ < kSafetyPollInterval) {
            return;
        }
        lastCheck_ = now;
    }

    std::string nextReason;
    if (!core::Jvm::isReady() && !core::Jvm::initialize()) {
        nextReason = "JVM/JVMTI is not ready";
        std::lock_guard lock(mutex_);
        if (state_ != SafetyState::Unsafe || reason_ != nextReason) {
            util::logWarning("Safety state: unsafe (JVM/JVMTI is not ready)");
        }
        state_ = SafetyState::Unsafe;
        reason_ = nextReason;
        return;
    }

    const bool safe = sdk::Minecraft::instance().isSingleplayerWorldSafe(&nextReason);
    const SafetyState nextState = safe ? SafetyState::SafeSingleplayer : SafetyState::Unsafe;

    std::lock_guard lock(mutex_);
    if (nextState != state_ || nextReason != reason_) {
        std::ostringstream message;
        message << "Safety state: " << stateName(nextState) << " (" << nextReason << ")";
        if (nextState == SafetyState::SafeSingleplayer) {
            util::logInfo(message.str());
        } else {
            util::logWarning(message.str());
        }
    }

    state_ = nextState;
    reason_ = nextReason.empty() ? "No safety reason supplied" : std::move(nextReason);
}

bool SafetyGuard::isSingleplayerSafe() {
    update(false);
    std::lock_guard lock(mutex_);
    return state_ == SafetyState::SafeSingleplayer;
}

bool SafetyGuard::shouldDisableOverlay() {
    return !isSingleplayerSafe();
}

void SafetyGuard::shutdownModules() {
    modules::ModuleManager::instance().suspendForSafety();
}

SafetyState SafetyGuard::state() const {
    std::lock_guard lock(mutex_);
    return state_;
}

std::string SafetyGuard::reason() const {
    std::lock_guard lock(mutex_);
    return reason_;
}

} // namespace core
