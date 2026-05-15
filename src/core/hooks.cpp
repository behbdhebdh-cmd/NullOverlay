#include "core/hooks.h"

#include "core/jvm_wrapper.h"
#include "MinHook.h"
#include "render/renderer.h"
#include "util/logging.h"

namespace {

using WglSwapBuffersFn = BOOL(WINAPI*)(HDC);

std::atomic_bool g_unloadRequested{false};
std::atomic_bool g_installed{false};
std::atomic_uint64_t g_swapCallCount{0};
void* g_swapBuffersTarget = nullptr;
WglSwapBuffersFn g_originalSwapBuffers = nullptr;

using MHInitializeFn = MH_STATUS(WINAPI*)();
using MHUninitializeFn = MH_STATUS(WINAPI*)();
using MHCreateHookFn = MH_STATUS(WINAPI*)(LPVOID, LPVOID, LPVOID*);
using MHRemoveHookFn = MH_STATUS(WINAPI*)(LPVOID);
using MHEnableHookFn = MH_STATUS(WINAPI*)(LPVOID);
using MHDisableHookFn = MH_STATUS(WINAPI*)(LPVOID);
using MHStatusToStringFn = const char*(WINAPI*)(MH_STATUS);

struct MinHookRuntime {
    HMODULE module{};
    MHInitializeFn initialize{};
    MHUninitializeFn uninitialize{};
    MHCreateHookFn createHook{};
    MHRemoveHookFn removeHook{};
    MHEnableHookFn enableHook{};
    MHDisableHookFn disableHook{};
    MHStatusToStringFn statusToString{};
};

MinHookRuntime g_minHook;

std::filesystem::path currentModuleDirectory() {
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&currentModuleDirectory),
            &self)) {
        return {};
    }

    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(self, path, static_cast<DWORD>(std::size(path)));
    if (length == 0 || length >= std::size(path)) {
        return {};
    }

    return std::filesystem::path(path).parent_path();
}

template <typename Function>
bool loadExport(HMODULE module, const char* name, Function& target) {
    target = reinterpret_cast<Function>(GetProcAddress(module, name));
    if (target != nullptr) {
        return true;
    }

    std::string message("Missing MinHook export: ");
    message.append(name);
    util::logError(message);
    return false;
}

bool loadMinHookRuntime() {
    if (g_minHook.module != nullptr) {
        util::logInfo("MinHook runtime already loaded");
        return true;
    }

    const std::filesystem::path localPath = currentModuleDirectory() / L"MinHook.x64.dll";
    util::logInfo("Trying to load MinHook from: " + localPath.string());
    HMODULE module = LoadLibraryW(localPath.c_str());
    if (module == nullptr) {
        std::ostringstream fallback;
        fallback << "Local MinHook load failed, GetLastError=" << GetLastError() << "; trying default DLL search path";
        util::logWarning(fallback.str());
        module = LoadLibraryW(L"MinHook.x64.dll");
    }

    if (module == nullptr) {
        std::ostringstream message;
        message << "MinHook.x64.dll could not be loaded, GetLastError=" << GetLastError();
        util::logError(message.str());
        return false;
    }

    MinHookRuntime runtime{};
    runtime.module = module;
    if (!loadExport(module, "MH_Initialize", runtime.initialize) ||
        !loadExport(module, "MH_Uninitialize", runtime.uninitialize) ||
        !loadExport(module, "MH_CreateHook", runtime.createHook) ||
        !loadExport(module, "MH_RemoveHook", runtime.removeHook) ||
        !loadExport(module, "MH_EnableHook", runtime.enableHook) ||
        !loadExport(module, "MH_DisableHook", runtime.disableHook) ||
        !loadExport(module, "MH_StatusToString", runtime.statusToString)) {
        FreeLibrary(module);
        return false;
    }

    g_minHook = runtime;
    std::ostringstream message;
    message << "MinHook runtime loaded dynamically. module=" << runtime.module;
    util::logInfo(message.str());
    return true;
}

void unloadMinHookRuntime() {
    if (g_minHook.module != nullptr) {
        FreeLibrary(g_minHook.module);
        g_minHook = {};
    }
}

std::string minHookStatus(MH_STATUS status) {
    if (g_minHook.statusToString != nullptr) {
        return g_minHook.statusToString(status);
    }

    std::ostringstream stream;
    stream << "MinHook status " << static_cast<int>(status);
    return stream.str();
}

void logSehException(DWORD code) {
    std::ostringstream message;
    message << "SEH exception in wglSwapBuffers overlay path, code=0x" << std::hex << code;
    util::logError(message.str());
}

void runOverlayProtected(HDC deviceContext) {
    __try {
        render::Renderer::instance().onSwapBuffers(deviceContext);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        logSehException(GetExceptionCode());
        g_unloadRequested.store(true, std::memory_order_release);
    }
}

BOOL WINAPI hookedWglSwapBuffers(HDC deviceContext) {
    const auto swapCall = g_swapCallCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (swapCall <= 10 || swapCall % 300 == 0) {
        std::ostringstream message;
        message << "hookedWglSwapBuffers call #" << swapCall
                << ", HDC=" << deviceContext
                << ", currentHGLRC=" << wglGetCurrentContext()
                << ", unloadRequested=" << (g_unloadRequested.load(std::memory_order_acquire) ? "true" : "false");
        util::logInfo(message.str());
    }

    if (g_unloadRequested.load(std::memory_order_acquire)) {
        render::Renderer::instance().shutdown();
    } else {
        runOverlayProtected(deviceContext);
    }

    core::Jvm::detachCurrentThreadIfOwned();
    return g_originalSwapBuffers(deviceContext);
}

} // namespace

namespace core::Hooks {

bool install() {
    util::logInfo("Hooks::install entered");
    if (g_installed.load(std::memory_order_acquire)) {
        util::logInfo("Hooks::install skipped because hooks are already installed");
        return true;
    }

    HMODULE opengl = GetModuleHandleW(L"opengl32.dll");
    std::ostringstream openglMessage;
    openglMessage << "GetModuleHandle(opengl32.dll)=" << opengl;
    util::logInfo(openglMessage.str());
    if (opengl == nullptr) {
        opengl = LoadLibraryW(L"opengl32.dll");
        std::ostringstream loadMessage;
        loadMessage << "LoadLibrary(opengl32.dll)=" << opengl << ", GetLastError=" << GetLastError();
        util::logInfo(loadMessage.str());
    }

    if (opengl == nullptr) {
        util::logError("opengl32.dll could not be loaded");
        return false;
    }

    g_swapBuffersTarget = reinterpret_cast<void*>(GetProcAddress(opengl, "wglSwapBuffers"));
    std::ostringstream targetMessage;
    targetMessage << "opengl32!wglSwapBuffers target=" << g_swapBuffersTarget;
    util::logInfo(targetMessage.str());
    if (g_swapBuffersTarget == nullptr) {
        util::logError("opengl32!wglSwapBuffers was not found");
        return false;
    }

    if (!loadMinHookRuntime()) {
        return false;
    }

    MH_STATUS status = g_minHook.initialize();
    util::logInfo("MH_Initialize returned: " + minHookStatus(status));
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        util::logError("MH_Initialize failed: " + minHookStatus(status));
        unloadMinHookRuntime();
        return false;
    }

    status = g_minHook.createHook(
        g_swapBuffersTarget,
        reinterpret_cast<void*>(&hookedWglSwapBuffers),
        reinterpret_cast<void**>(&g_originalSwapBuffers));
    std::ostringstream createMessage;
    createMessage << "MH_CreateHook(wglSwapBuffers) returned: " << minHookStatus(status)
                  << ", original=" << reinterpret_cast<void*>(g_originalSwapBuffers)
                  << ", detour=" << reinterpret_cast<void*>(&hookedWglSwapBuffers);
    util::logInfo(createMessage.str());
    if (status != MH_OK) {
        util::logError("MH_CreateHook(wglSwapBuffers) failed: " + minHookStatus(status));
        g_minHook.uninitialize();
        unloadMinHookRuntime();
        return false;
    }

    status = g_minHook.enableHook(g_swapBuffersTarget);
    util::logInfo("MH_EnableHook(wglSwapBuffers) returned: " + minHookStatus(status));
    if (status != MH_OK) {
        util::logError("MH_EnableHook(wglSwapBuffers) failed: " + minHookStatus(status));
        g_minHook.removeHook(g_swapBuffersTarget);
        g_minHook.uninitialize();
        unloadMinHookRuntime();
        return false;
    }

    g_unloadRequested.store(false, std::memory_order_release);
    g_swapCallCount.store(0, std::memory_order_release);
    g_installed.store(true, std::memory_order_release);
    util::logInfo("wglSwapBuffers hook installed");
    return true;
}

void uninstall() {
    util::logInfo("Hooks::uninstall entered");
    g_unloadRequested.store(true, std::memory_order_release);

    if (g_installed.exchange(false, std::memory_order_acq_rel)) {
        if (g_swapBuffersTarget != nullptr && g_minHook.module != nullptr) {
            g_minHook.disableHook(g_swapBuffersTarget);
            g_minHook.removeHook(g_swapBuffersTarget);
        }
        render::Renderer::instance().shutdown();
        if (g_minHook.module != nullptr) {
            g_minHook.uninitialize();
        }
        unloadMinHookRuntime();
        util::logInfo("Hooks uninstalled");
    }

    g_swapBuffersTarget = nullptr;
    g_originalSwapBuffers = nullptr;
}

void requestUnload() {
    g_unloadRequested.store(true, std::memory_order_release);
}

bool isUnloadRequested() {
    return g_unloadRequested.load(std::memory_order_acquire);
}

} // namespace core::Hooks
