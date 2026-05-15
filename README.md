# NullOverlay

NullOverlay is a native Windows x64 C++20 OpenGL/ImGui overlay framework for local Minecraft Java Edition Forge 1.21.1 debugging.

The project is intentionally fail-closed. Rendering and modules only run after the runtime guard verifies an integrated singleplayer server, a local client world, a local player, and no remote server metadata. If that validation fails, modules are suspended and entity caches are cleared.

No injector, multiplayer support, networking, packet editing, evasion, persistence, driver code, or gameplay patching code is included.

## Features

- OpenGL `wglSwapBuffers` hook for drawing an ImGui overlay.
- Runtime menu toggled with `INSERT`.
- Graceful unload with `DELETE` or the menu unload button.
- Entity debug visualizer for local singleplayer worlds.
- Configurable ESP-style labels, health bars, boxes, markers, snaplines, and category labels.
- Optional FPS counter and fullbright toggle.
- JNI/JVMTI wrapper for reading Minecraft client state.
- Safety guard that disables overlay behavior outside verified integrated singleplayer.
- Dynamic MinHook loading from the same directory as the built DLL.

## Repository Layout

```text
.
|-- CMakeLists.txt              CMake project for the NullOverlay shared library.
|-- README.md                   Project overview, build notes, and code map.
|-- external/                   Optional dependency drop-in folder.
|   `-- README.md               Dependency layout expected by CMake.
|-- imgui-1.92.8/               Vendored Dear ImGui source used as a fallback.
|-- MinHook_134_bin/            Vendored MinHook binary/include fallback.
`-- src/
    |-- dllmain.cpp             DLL entry point, startup thread, shutdown flow.
    |-- pch.h                   Common Windows, STL, OpenGL, JNI, and ImGui includes.
    |-- config/
    |   `-- mappings.h          Minecraft/Forge class, method, and field mappings.
    |-- core/
    |   |-- hooks.*             MinHook loading and `wglSwapBuffers` hook lifecycle.
    |   |-- jvm_wrapper.*       JVM discovery, thread attach/detach, JNI helpers.
    |   `-- safety_guard.*      Singleplayer safety checks and module suspension.
    |-- modules/
    |   |-- module.h            Base module interface.
    |   |-- module_manager.*    Module registration, update, render, and safety state.
    |   `-- entity_overlay.*    Entity cache refresh and overlay render module.
    |-- render/
    |   |-- menu.*              ImGui menu, settings UI, FPS HUD, unload control.
    |   |-- overlay_renderer.*  2D projected entity drawing.
    |   `-- renderer.*          ImGui/OpenGL initialization and per-frame rendering.
    |-- sdk/
    |   |-- entity.*            Entity snapshot extraction and categorization.
    |   |-- minecraft.*         Minecraft singleton access, camera, world, fullbright.
    |   `-- world.*             Client world traversal and entity collection.
    `-- util/
        |-- logging.*           Runtime logging helpers.
        `-- math.*              Vector, matrix, and projection math helpers.
```

Generated build outputs belong in `build/` and are ignored by Git.

## Dependencies

Required tools:

- Windows x64.
- Visual Studio 2022 C++ x64 toolchain.
- CMake 3.24 or newer.
- JDK headers available through `JAVA_HOME` or CMake `FindJNI`.
- Dear ImGui source tree.
- MinHook x64 import library and DLL.

Dependency lookup order:

1. `external/imgui` containing `imgui.h` and `backends/`.
2. `imgui-1.92.8/imgui-1.92.8`.
3. `external/minhook` containing `include/MinHook.h` and `bin/MinHook.x64.lib`.
4. `MinHook_134_bin`.

The current repository includes compatible fallback vendor folders. You can also replace them by placing your own dependency copies in `external/`.

## Build

Open a Visual Studio Developer PowerShell or Developer Command Prompt, then run:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The output DLL is created under the configured build directory. `MinHook.x64.dll` is copied next to the built `NullOverlay.dll` when the MinHook DLL is available.

## Runtime

Keys:

- `INSERT`: toggle the menu.
- `DELETE`: request graceful unload.

Injection notes:

- Inject only `NullOverlay.dll`.
- Do not inject `MinHook.x64.dll` as a second entry.
- Keep `MinHook.x64.dll` in the same folder as `NullOverlay.dll`.

`NullOverlay.dll` dynamically loads MinHook after Windows has already loaded the overlay DLL. This avoids dependency-search failures in injectors that only call `LoadLibrary` on the selected DLL.

## Safety Model

The safety model is implemented in `src/core/safety_guard.*` and `src/sdk/minecraft.*`.

Before modules render, the overlay verifies:

- The JVM/JVMTI bridge is ready.
- Minecraft can be resolved through the configured mappings.
- An integrated singleplayer server is active.
- A client world and local player are present.
- Remote server metadata is absent.

If any check fails, the overlay treats the runtime as unsafe, clears cached entity data, and suspends module behavior.

## Code Flow

1. `src/dllmain.cpp` starts a worker thread from `DllMain`.
2. The worker initializes logging, safety checks, default modules, and hooks.
3. `src/core/hooks.cpp` dynamically loads MinHook and hooks `opengl32!wglSwapBuffers`.
4. Each swap-buffers call enters `src/render/renderer.cpp`.
5. The renderer initializes ImGui/OpenGL state and draws the menu/HUD/overlay.
6. Modules are updated through `src/modules/module_manager.cpp`.
7. The entity visualizer reads Minecraft state through `src/sdk/*` and projects entities through `src/util/math.*`.
8. Shutdown unhooks MinHook, tears down ImGui/OpenGL state, detaches JVM threads, and frees the DLL.

## Updating Minecraft Mappings

Mappings live in `src/config/mappings.h`.

When Minecraft, Forge, or mappings change, update the class names, method names, signatures, and field names there first. Most runtime lookup failures will appear in the overlay log through the helpers in `src/util/logging.*`.

## Logging

Runtime logging is initialized in `src/util/logging.*` and started from `src/dllmain.cpp`.

Use the logs to diagnose:

- JVM/JVMTI initialization problems.
- Missing Minecraft classes, methods, or fields.
- Safety-state changes.
- MinHook loading and hook installation.
- ImGui/OpenGL initialization.

## Contributing

Keep changes focused and easy to review:

- Put Minecraft mapping updates in `src/config/mappings.h`.
- Put rendering/UI work in `src/render/`.
- Put Minecraft state readers in `src/sdk/`.
- Put module behavior in `src/modules/`.
- Keep build outputs out of commits.
- Build with warnings enabled before opening a pull request.

## License

No license file is included yet. Add a license before accepting outside contributions or publishing official releases.
