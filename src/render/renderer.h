#pragma once

#include "pch.h"

namespace render {

class Renderer {
public:
    static Renderer& instance();

    bool initialize(HDC deviceContext);
    void shutdown();
    void onSwapBuffers(HDC deviceContext);
    bool isInitialized() const;

    HWND window() const { return window_; }

private:
    Renderer() = default;

    static LRESULT CALLBACK staticWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool beginFrame();
    void endFrame();
    void restoreWndProc();

    mutable std::recursive_mutex mutex_;
    HWND window_{};
    WNDPROC originalWndProc_{};
    bool initialized_{};
    bool inFrame_{};
};

} // namespace render
