#include "pch.h"

#include "core/hooks.h"
#include "core/jvm_wrapper.h"
#include "core/safety_guard.h"
#include "modules/module_manager.h"
#include "render/renderer.h"
#include "sdk/minecraft.h"
#include "util/logging.h"

namespace {

HMODULE g_module = nullptr;

DWORD WINAPI mainThread(LPVOID) {
    util::initializeLogging();
    util::logInfo("NullOverlay starting");

    core::SafetyGuard::instance().initialize();
    util::logInfo("SafetyGuard initialized");
    modules::ModuleManager::instance().initializeDefaults();
    util::logInfo("Default modules registered");

    if (!core::Hooks::install()) {
        util::logError("Hook installation failed; unloading");
        modules::ModuleManager::instance().shutdown();
        core::SafetyGuard::instance().shutdown();
        sdk::Minecraft::instance().shutdown();
        core::Jvm::shutdown();
        util::shutdownLogging();
        FreeLibraryAndExitThread(g_module, 1);
    }

    util::logInfo("Startup completed; waiting for unload request");
    while (!core::Hooks::isUnloadRequested()) {
        Sleep(50);
    }

    util::logInfo("Unload requested");
    for (int i = 0; i < 120 && render::Renderer::instance().isInitialized(); ++i) {
        Sleep(16);
    }
    core::Hooks::uninstall();
    modules::ModuleManager::instance().shutdown();
    core::SafetyGuard::instance().shutdown();
    sdk::Minecraft::instance().shutdown();
    core::Jvm::detachCurrentThreadIfOwned();
    core::Jvm::shutdown();
    util::logInfo("NullOverlay stopped");
    util::shutdownLogging();

    FreeLibraryAndExitThread(g_module, 0);
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);

        HANDLE thread = CreateThread(nullptr, 0, mainThread, nullptr, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    }

    return TRUE;
}
