#pragma once

#include "pch.h"

namespace core {

enum class SafetyState {
    Unknown,
    SafeSingleplayer,
    Unsafe,
};

class SafetyGuard {
public:
    static SafetyGuard& instance();

    void initialize();
    void shutdown();
    void update(bool force = false);

    bool isSingleplayerSafe();
    bool shouldDisableOverlay();
    void shutdownModules();

    SafetyState state() const;
    std::string reason() const;

private:
    SafetyGuard() = default;

    mutable std::mutex mutex_;
    SafetyState state_{SafetyState::Unknown};
    std::string reason_{"Not yet verified"};
    std::chrono::steady_clock::time_point lastCheck_{};
};

} // namespace core
