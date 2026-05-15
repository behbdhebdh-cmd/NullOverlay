#include "render/renderer.h"

#include "core/hooks.h"
#include "core/safety_guard.h"
#include "modules/module_manager.h"
#include "render/menu.h"
#include "render/overlay_renderer.h"
#include "sdk/minecraft.h"
#include "util/logging.h"

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

using GlBlendEquationFn = void(APIENTRY*)(GLenum);
using GlBindFramebufferFn = void(APIENTRY*)(GLenum, GLuint);
constexpr GLenum kGlFuncAdd = 0x8006;
constexpr GLenum kGlDrawFramebuffer = 0x8CA9;
constexpr GLenum kGlDrawFramebufferBinding = 0x8CA6;
constexpr GLenum kGlShadingLanguageVersion = 0x8B8C;
std::uint64_t g_renderFrameCount = 0;

bool shouldLogFrame(std::uint64_t frame) {
    return frame <= 120 || frame % 300 == 0;
}

const char* glString(GLenum name) {
    const auto* value = glGetString(name);
    return value != nullptr ? reinterpret_cast<const char*>(value) : "<null>";
}

std::string hwndSummary(HWND hwnd) {
    if (hwnd == nullptr) {
        return "HWND=null";
    }

    char title[256]{};
    char className[128]{};
    GetWindowTextA(hwnd, title, static_cast<int>(std::size(title)));
    GetClassNameA(hwnd, className, static_cast<int>(std::size(className)));

    DWORD processId = 0;
    const DWORD threadId = GetWindowThreadProcessId(hwnd, &processId);
    RECT rect{};
    GetClientRect(hwnd, &rect);

    std::ostringstream message;
    message << "HWND=" << hwnd
            << ", title=\"" << title << "\""
            << ", class=\"" << className << "\""
            << ", windowThreadId=" << threadId
            << ", processId=" << processId
            << ", client=" << (rect.right - rect.left) << "x" << (rect.bottom - rect.top);
    return message.str();
}

GlBlendEquationFn resolveGlBlendEquation() {
    static GlBlendEquationFn fn = []() -> GlBlendEquationFn {
        auto pointer = reinterpret_cast<GlBlendEquationFn>(wglGetProcAddress("glBlendEquation"));
        if (pointer != nullptr) {
            return pointer;
        }
        HMODULE opengl = GetModuleHandleW(L"opengl32.dll");
        if (opengl == nullptr) {
            return nullptr;
        }
        return reinterpret_cast<GlBlendEquationFn>(GetProcAddress(opengl, "glBlendEquation"));
    }();
    return fn;
}

bool isValidWglProcAddress(void* pointer) {
    const auto value = reinterpret_cast<std::uintptr_t>(pointer);
    return pointer != nullptr && value > 3 && value != static_cast<std::uintptr_t>(-1);
}

GlBindFramebufferFn resolveGlBindFramebuffer() {
    static GlBindFramebufferFn fn = []() -> GlBindFramebufferFn {
        void* pointer = reinterpret_cast<void*>(wglGetProcAddress("glBindFramebuffer"));
        if (isValidWglProcAddress(pointer)) {
            std::ostringstream message;
            message << "Resolved glBindFramebuffer via wglGetProcAddress: " << pointer;
            util::logInfo(message.str());
            return reinterpret_cast<GlBindFramebufferFn>(pointer);
        }
        std::ostringstream message;
        message << "glBindFramebuffer could not be resolved. raw wglGetProcAddress value=" << pointer;
        util::logWarning(message.str());
        return nullptr;
    }();
    return fn;
}

class ScopedDefaultDrawFramebuffer {
public:
    ScopedDefaultDrawFramebuffer() {
        bindFramebuffer_ = resolveGlBindFramebuffer();
        if (bindFramebuffer_ == nullptr) {
            return;
        }

        glGetIntegerv(kGlDrawFramebufferBinding, &previousDrawFramebuffer_);
        if (previousDrawFramebuffer_ != 0) {
            static bool logged = false;
            if (!logged) {
                std::ostringstream message;
                message << "Non-default draw framebuffer detected at swap: " << previousDrawFramebuffer_
                        << ". Rendering overlay to default backbuffer, then restoring previous framebuffer.";
                util::logInfo(message.str());
                logged = true;
            }
            bindFramebuffer_(kGlDrawFramebuffer, 0);
        }
    }

    ~ScopedDefaultDrawFramebuffer() {
        if (bindFramebuffer_ != nullptr && previousDrawFramebuffer_ != 0) {
            bindFramebuffer_(kGlDrawFramebuffer, static_cast<GLuint>(previousDrawFramebuffer_));
        }
    }

    ScopedDefaultDrawFramebuffer(const ScopedDefaultDrawFramebuffer&) = delete;
    ScopedDefaultDrawFramebuffer& operator=(const ScopedDefaultDrawFramebuffer&) = delete;

private:
    GlBindFramebufferFn bindFramebuffer_{};
    GLint previousDrawFramebuffer_{};
};

bool isMouseMessage(UINT message) {
    switch (message) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        return true;
    default:
        return false;
    }
}

bool isKeyboardMessage(UINT message) {
    switch (message) {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
        return true;
    default:
        return false;
    }
}

} // namespace

namespace render {

Renderer& Renderer::instance() {
    static Renderer renderer;
    return renderer;
}

bool Renderer::initialize(HDC deviceContext) {
    std::lock_guard lock(mutex_);
    if (initialized_) {
        return true;
    }

    {
        std::ostringstream message;
        message << "Renderer::initialize entered. incomingHDC=" << deviceContext
                << ", currentHDC=" << wglGetCurrentDC()
                << ", currentHGLRC=" << wglGetCurrentContext();
        util::logInfo(message.str());
    }

    HWND hwnd = WindowFromDC(deviceContext);
    util::logInfo("WindowFromDC result: " + hwndSummary(hwnd));
    if (hwnd == nullptr) {
        util::logWarning("Renderer initialization skipped because WindowFromDC returned null");
        return false;
    }

    {
        const int pixelFormat = GetPixelFormat(deviceContext);
        std::ostringstream message;
        message << "OpenGL context before ImGui init: pixelFormat=" << pixelFormat
                << ", vendor=" << glString(GL_VENDOR)
                << ", renderer=" << glString(GL_RENDERER)
                << ", version=" << glString(GL_VERSION)
                << ", shadingLanguage=" << glString(kGlShadingLanguageVersion);
        util::logInfo(message.str());
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.Fonts->Clear();
    ImFontConfig fontConfig{};
    fontConfig.SizePixels = 18.0f;
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &fontConfig);
    if (font == nullptr) {
        util::logWarning("Failed to load Segoe UI font; falling back to Dear ImGui default font");
        io.Fonts->AddFontDefault();
    } else {
        std::ostringstream message;
        message << "Loaded Segoe UI font for ImGui. SizePixels=" << fontConfig.SizePixels;
        util::logInfo(message.str());
    }

    Menu::instance().applyStyle();

    if (!ImGui_ImplWin32_InitForOpenGL(hwnd)) {
        util::logError("ImGui Win32 backend initialization failed");
        ImGui::DestroyContext();
        return false;
    }
    util::logInfo("ImGui Win32 backend initialized for OpenGL");

    if (!ImGui_ImplOpenGL3_Init("#version 150")) {
        util::logError("ImGui OpenGL3 backend initialization failed");
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    util::logInfo("ImGui OpenGL3 backend initialized with GLSL #version 150");

    window_ = hwnd;
    SetLastError(0);
    originalWndProc_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Renderer::staticWndProc)));
    const DWORD subclassError = GetLastError();
    if (originalWndProc_ == nullptr && subclassError != ERROR_SUCCESS) {
        std::ostringstream message;
        message << "Failed to subclass Minecraft window procedure; input capture is limited. GetLastError=" << subclassError;
        util::logWarning(message.str());
    } else {
        std::ostringstream message;
        message << "Minecraft window procedure subclassed. originalWndProc=" << reinterpret_cast<void*>(originalWndProc_)
                << ", newWndProc=" << reinterpret_cast<void*>(&Renderer::staticWndProc);
        util::logInfo(message.str());
    }

    initialized_ = true;
    util::logInfo(std::string("Renderer initialized successfully. Menu visible=") + (Menu::instance().isVisible() ? "true" : "false"));
    return true;
}

void Renderer::shutdown() {
    std::lock_guard lock(mutex_);
    if (!initialized_) {
        return;
    }

    restoreWndProc();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    initialized_ = false;
    inFrame_ = false;
    window_ = nullptr;
    util::logInfo("Renderer shut down. ImGui backends and context destroyed.");
}

bool Renderer::isInitialized() const {
    std::lock_guard lock(mutex_);
    return initialized_;
}

void Renderer::onSwapBuffers(HDC deviceContext) {
    std::lock_guard lock(mutex_);

    if (core::Hooks::isUnloadRequested()) {
        static bool loggedUnloadSkip = false;
        if (!loggedUnloadSkip) {
            util::logInfo("Renderer::onSwapBuffers skipped because unload was requested");
            loggedUnloadSkip = true;
        }
        return;
    }

    if (!initialized_ && !initialize(deviceContext)) {
        util::logWarning("Renderer initialization deferred or failed");
        return;
    }

    core::SafetyGuard::instance().update(false);
    const bool singleplayerSafe = core::SafetyGuard::instance().isSingleplayerSafe();
    if (!singleplayerSafe) {
        core::SafetyGuard::instance().shutdownModules();
    }

    RenderContext context{};
    RECT rect{};
    if (window_ != nullptr && GetClientRect(window_, &rect)) {
        context.width = rect.right - rect.left;
        context.height = rect.bottom - rect.top;
    }

    if (singleplayerSafe) {
        sdk::Minecraft::instance().applyFullbright(Menu::instance().settings().fullbrightEnabled);

        if (!sdk::Minecraft::instance().getWindowSize(context.width, context.height)) {
            if (window_ != nullptr && GetClientRect(window_, &rect)) {
                context.width = rect.right - rect.left;
                context.height = rect.bottom - rect.top;
            }
        }

        if (!sdk::Minecraft::instance().getCamera(context.camera)) {
            core::SafetyGuard::instance().shutdownModules();
            context.camera = {};
        }
    }
    if (!singleplayerSafe) {
        sdk::Minecraft::instance().applyFullbright(false);
    }

    const std::uint64_t nextFrame = g_renderFrameCount + 1;
    if (shouldLogFrame(nextFrame)) {
        std::ostringstream message;
        message << "Renderer frame #" << nextFrame
                << ": singleplayerSafe=" << (singleplayerSafe ? "true" : "false")
                << ", menuVisible=" << (Menu::instance().isVisible() ? "true" : "false")
                << ", clientSize=" << context.width << "x" << context.height
                << ", cameraValid=" << (context.camera.valid ? "true" : "false")
                << ", safetyReason=\"" << core::SafetyGuard::instance().reason() << "\"";
        util::logInfo(message.str());
    }

    if (!beginFrame()) {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    context.width = static_cast<int>(std::max(1.0f, io.DisplaySize.x));
    context.height = static_cast<int>(std::max(1.0f, io.DisplaySize.y));

    Menu::instance().render(core::SafetyGuard::instance().reason());
    if (singleplayerSafe && context.camera.valid) {
        modules::ModuleManager::instance().renderAll(context);
    }
    Menu::instance().renderHud();
    endFrame();

    if (core::Hooks::isUnloadRequested()) {
        shutdown();
    }
}

LRESULT CALLBACK Renderer::staticWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return Renderer::instance().wndProc(hwnd, message, wParam, lParam);
}

LRESULT Renderer::wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_KEYUP) {
        if (wParam == VK_INSERT) {
            Menu::instance().toggleVisible();
            util::logInfo(std::string("VK_INSERT released. Menu visible now=") + (Menu::instance().isVisible() ? "true" : "false"));
            return 0;
        }

        if (wParam == VK_DELETE) {
            util::logInfo("VK_DELETE released. Requesting overlay unload.");
            core::Hooks::requestUnload();
            return 0;
        }
    }

    const bool menuVisible = Menu::instance().isVisible();
    if (initialized_ && menuVisible) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam)) {
            return true;
        }

        const ImGuiIO& io = ImGui::GetIO();
        if ((io.WantCaptureMouse && isMouseMessage(message)) ||
            (io.WantCaptureKeyboard && isKeyboardMessage(message))) {
            static std::uint64_t capturedMessages = 0;
            ++capturedMessages;
            if (capturedMessages <= 20 || capturedMessages % 200 == 0) {
                std::ostringstream captureMessage;
                captureMessage << "WndProc captured message #" << capturedMessages
                               << ": msg=0x" << std::hex << message << std::dec
                               << ", WantCaptureMouse=" << (io.WantCaptureMouse ? "true" : "false")
                               << ", WantCaptureKeyboard=" << (io.WantCaptureKeyboard ? "true" : "false");
                util::logInfo(captureMessage.str());
            }
            return true;
        }
    }

    if (originalWndProc_ != nullptr) {
        return CallWindowProcW(originalWndProc_, hwnd, message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool Renderer::beginFrame() {
    if (!initialized_ || inFrame_) {
        std::ostringstream message;
        message << "Renderer::beginFrame refused. initialized=" << (initialized_ ? "true" : "false")
                << ", inFrame=" << (inFrame_ ? "true" : "false");
        util::logWarning(message.str());
        return false;
    }

    const GLenum errorBeforeFrame = glGetError();
    if (errorBeforeFrame != GL_NO_ERROR) {
        std::ostringstream message;
        message << "OpenGL error before ImGui NewFrame: 0x" << std::hex << errorBeforeFrame;
        util::logWarning(message.str());
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();

    if (window_ != nullptr) {
        RECT rect{};
        if (GetClientRect(window_, &rect)) {
            ImGuiIO& io = ImGui::GetIO();
            io.DisplaySize = ImVec2(
                static_cast<float>(std::max<LONG>(1, rect.right - rect.left)),
                static_cast<float>(std::max<LONG>(1, rect.bottom - rect.top)));
            io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        }
    }

    ImGui::NewFrame();

    const std::uint64_t nextFrame = g_renderFrameCount + 1;
    if (shouldLogFrame(nextFrame)) {
        const ImGuiIO& io = ImGui::GetIO();
        std::ostringstream message;
        message << "ImGui NewFrame OK for frame #" << nextFrame
                << ": DisplaySize=" << io.DisplaySize.x << "x" << io.DisplaySize.y
                << ", DisplayFramebufferScale=" << io.DisplayFramebufferScale.x << "x" << io.DisplayFramebufferScale.y
                << ", DeltaTime=" << io.DeltaTime
                << ", WantCaptureMouse=" << (io.WantCaptureMouse ? "true" : "false")
                << ", WantCaptureKeyboard=" << (io.WantCaptureKeyboard ? "true" : "false");
        util::logInfo(message.str());
    }

    inFrame_ = true;
    return true;
}

void Renderer::endFrame() {
    if (!inFrame_) {
        return;
    }

    ImGui::Render();
    ++g_renderFrameCount;

    GLint viewport[4]{};
    GLint drawFramebuffer = 0;
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(kGlDrawFramebufferBinding, &drawFramebuffer);
    ImDrawData* drawData = ImGui::GetDrawData();
    if (shouldLogFrame(g_renderFrameCount)) {
        std::ostringstream message;
        message << "ImGui Render for frame #" << g_renderFrameCount
                << ": menuVisible=" << (Menu::instance().isVisible() ? "true" : "false")
                << ", drawData=" << drawData
                << ", cmdLists=" << (drawData != nullptr ? drawData->CmdListsCount : -1)
                << ", totalVtx=" << (drawData != nullptr ? drawData->TotalVtxCount : -1)
                << ", totalIdx=" << (drawData != nullptr ? drawData->TotalIdxCount : -1)
                << ", displayPos=" << (drawData != nullptr ? drawData->DisplayPos.x : 0.0f)
                << "," << (drawData != nullptr ? drawData->DisplayPos.y : 0.0f)
                << ", displaySize=" << (drawData != nullptr ? drawData->DisplaySize.x : 0.0f)
                << "x" << (drawData != nullptr ? drawData->DisplaySize.y : 0.0f)
                << ", framebufferScale=" << (drawData != nullptr ? drawData->FramebufferScale.x : 0.0f)
                << "x" << (drawData != nullptr ? drawData->FramebufferScale.y : 0.0f)
                << ", viewport=" << viewport[0] << "," << viewport[1] << "," << viewport[2] << "x" << viewport[3]
                << ", drawFramebuffer=" << drawFramebuffer;
        util::logInfo(message.str());
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    if (const auto glBlendEquationFn = resolveGlBlendEquation()) {
        glBlendEquationFn(kGlFuncAdd);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);

    ScopedDefaultDrawFramebuffer defaultDrawFramebuffer;
    ImGui_ImplOpenGL3_RenderDrawData(drawData);

    const GLenum errorAfterRender = glGetError();
    if (errorAfterRender != GL_NO_ERROR || shouldLogFrame(g_renderFrameCount)) {
        std::ostringstream message;
        message << "ImGui OpenGL render finished for frame #" << g_renderFrameCount
                << ", glGetError=0x" << std::hex << errorAfterRender;
        if (errorAfterRender == GL_NO_ERROR) {
            util::logInfo(message.str());
        } else {
            util::logWarning(message.str());
        }
    }

    inFrame_ = false;
}

void Renderer::restoreWndProc() {
    if (window_ == nullptr || originalWndProc_ == nullptr) {
        return;
    }

    const auto current = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window_, GWLP_WNDPROC));
    if (current == &Renderer::staticWndProc) {
        SetWindowLongPtrW(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalWndProc_));
    }
    originalWndProc_ = nullptr;
}

} // namespace render
