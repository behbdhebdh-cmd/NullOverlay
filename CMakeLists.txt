cmake_minimum_required(VERSION 3.24)

project(NullOverlay LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "NullOverlay is a native Windows x64 DLL project.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Configure this project with a 64-bit generator/toolchain.")
endif()

set(IMGUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui" CACHE PATH "Dear ImGui root containing imgui.h")
if(NOT EXISTS "${IMGUI_ROOT}/imgui.h")
    set(IMGUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/imgui-1.92.8/imgui-1.92.8" CACHE PATH "Dear ImGui root containing imgui.h" FORCE)
endif()

set(MINHOOK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/minhook" CACHE PATH "MinHook root containing include/MinHook.h and bin/MinHook.x64.lib")
if(NOT EXISTS "${MINHOOK_ROOT}/include/MinHook.h")
    set(MINHOOK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/MinHook_134_bin" CACHE PATH "MinHook root containing include/MinHook.h and bin/MinHook.x64.lib" FORCE)
endif()

if(NOT EXISTS "${IMGUI_ROOT}/imgui.h")
    message(FATAL_ERROR "Dear ImGui was not found. Set IMGUI_ROOT to a folder containing imgui.h.")
endif()

if(NOT EXISTS "${MINHOOK_ROOT}/include/MinHook.h")
    message(FATAL_ERROR "MinHook was not found. Set MINHOOK_ROOT to a folder containing include/MinHook.h.")
endif()

if(NOT EXISTS "${MINHOOK_ROOT}/bin/MinHook.x64.lib")
    message(FATAL_ERROR "MinHook x64 import library was not found at ${MINHOOK_ROOT}/bin/MinHook.x64.lib.")
endif()

find_package(JNI REQUIRED)

add_library(NullOverlay SHARED
    src/pch.h
    src/dllmain.cpp
    src/core/hooks.h
    src/core/hooks.cpp
    src/core/jvm_wrapper.h
    src/core/jvm_wrapper.cpp
    src/core/safety_guard.h
    src/core/safety_guard.cpp
    src/render/renderer.h
    src/render/renderer.cpp
    src/render/menu.h
    src/render/menu.cpp
    src/render/overlay_renderer.h
    src/render/overlay_renderer.cpp
    src/sdk/minecraft.h
    src/sdk/minecraft.cpp
    src/sdk/world.h
    src/sdk/world.cpp
    src/sdk/entity.h
    src/sdk/entity.cpp
    src/modules/module.h
    src/modules/module_manager.h
    src/modules/module_manager.cpp
    src/modules/entity_overlay.h
    src/modules/entity_overlay.cpp
    src/config/mappings.h
    src/util/math.h
    src/util/math.cpp
    src/util/logging.h
    src/util/logging.cpp
    "${IMGUI_ROOT}/imgui.cpp"
    "${IMGUI_ROOT}/imgui_draw.cpp"
    "${IMGUI_ROOT}/imgui_tables.cpp"
    "${IMGUI_ROOT}/imgui_widgets.cpp"
    "${IMGUI_ROOT}/backends/imgui_impl_win32.cpp"
    "${IMGUI_ROOT}/backends/imgui_impl_opengl3.cpp"
)

target_include_directories(NullOverlay PRIVATE
    src
    "${IMGUI_ROOT}"
    "${IMGUI_ROOT}/backends"
    "${MINHOOK_ROOT}/include"
    ${JNI_INCLUDE_DIRS}
)

target_compile_definitions(NullOverlay PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    NULL_OVERLAY_VERSION=\"0.1.0\"
)

if(MSVC)
    target_compile_options(NullOverlay PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(NullOverlay PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(NullOverlay PRIVATE
    opengl32
    user32
    gdi32
    imm32
    dwmapi
)

if(EXISTS "${MINHOOK_ROOT}/bin/MinHook.x64.dll")
    add_custom_command(TARGET NullOverlay POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${MINHOOK_ROOT}/bin/MinHook.x64.dll"
            "$<TARGET_FILE_DIR:NullOverlay>/MinHook.x64.dll"
    )
endif()
