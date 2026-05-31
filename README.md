# zonvras — Vulkan Shader Playground

A minimal Vulkan + C++ Shadertoy-inspired renderer.  
Fullscreen raymarching via a single fragment shader, fly camera, and hot-reload.

---

## Prerequisites

| Tool | Version |
|------|---------|
| [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) | 1.3+ (sets `VULKAN_SDK` env var) |
| CMake | 3.20+ |
| [Conan](https://conan.io/) | 2.x (`pip install conan`) |
| C++ compiler | MSVC 2022 / Clang 15+ |
| `glslc` | Included with Vulkan SDK |

---

## Build (Windows)

```bat
:: 1. Install dependencies with Conan
mkdir build
cd build
conan install .. --output-folder=. --build=missing -s build_type=Release

:: 2. Configure CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release

:: 3. Build
cmake --build . --config Release

:: 4. Run
Release\zonvras.exe
```

For a **Debug** build (enables Vulkan validation layers):

```bat
conan install .. --output-folder=. --build=missing -s build_type=Debug
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
Debug\zonvras.exe
```

---

## Controls

| Key / Input | Action |
|-------------|--------|
| Left-click  | Capture mouse / enable look |
| Escape      | Release mouse (click again to capture) / exit |
| W / S       | Move forward / backward |
| A / D       | Strafe left / right |
| Q / E       | Move down / up |
| Shift       | Fast movement (×3 speed) |
| Mouse       | Look around |

---

## Examples

Shader examples live under `shaders/examples/`. Each subdirectory is one example:

```
shaders/examples/
├── default/
│   └── main.frag
├── mandelbrot/
│   └── main.frag
└── mandelbulb/
    └── main.frag
```

At startup the app discovers all example folders and shows them in an **ImGui panel** (top-left corner). Click an entry to switch the active shader instantly.

To add a new example, create a folder under `shaders/examples/` with a `main.frag` and compile it (see below).

---

## Shader Hot-Reload

The app watches the active example's compiled `.spv` at runtime. To trigger a hot-reload:

1. Edit `shaders/examples/<name>/main.frag`
2. Recompile the SPIR-V:
   ```bat
   glslc shaders/examples/<name>/main.frag -o build/shaders/examples/<name>/main.frag.spv
   ```
3. The running app detects the `.spv` change and reloads the pipeline automatically — no restart needed.

> Tip: Set up a file watcher / task in VS Code to run `glslc` on save.

---

## Project Structure

```
zonvras/
├── CMakeLists.txt        # Build configuration
├── conanfile.py          # Conan 2 dependencies (glfw, glm, vulkan-headers, imgui)
├── README.md
├── shaders/
│   ├── fullscreen.vert              # Fullscreen triangle vertex shader
│   └── examples/
│       ├── default/main.frag        # Default raymarching scene
│       ├── mandelbrot/main.frag
│       └── mandelbulb/main.frag
└── src/
    ├── main.cpp                     # Entry point
    ├── app.h / app.cpp              # Main loop, example discovery, ImGui UI, hot-reload
    ├── vulkan_context.h / vulkan_context.cpp   # All Vulkan init/render
    ├── camera.h / camera.cpp                   # Fly camera
    ├── shader_watcher.h / shader_watcher.cpp   # File modification watcher
    └── shared_types.h               # PushConstants struct (shared CPU/GPU layout)
```

---

## Uniforms (Push Constants)

```glsl
layout(push_constant) uniform PC {
    float time;        // seconds since start
    // ...padding...
    vec2  resolution;  // viewport size in pixels
    // ...padding...
    vec3  camPos;      // world-space camera position
    vec4  camRot;      // camera orientation quaternion (w, x, y, z)
} pc;
```

Use `quatRotate(pc.camRot, localDir)` (already in the shader) to build world-space rays.

---

## Extending / Adding Shaders

Each example is a self-contained `main.frag` in its own folder under `shaders/examples/`.

The scene is defined entirely in the fragment shader:

- **`scene(vec3 p)`** — add / blend more SDFs here
- **`lighting(vec3 pos, vec3 rd)`** — tweak shading model
- **`sky(vec3 rd)`** — change background

To add a new shader, create `shaders/examples/<myshader>/main.frag` and compile it. The app will pick it up on next launch.

Useful SDF references: [Inigo Quilez — distfunctions](https://iquilezles.org/articles/distfunctions/)
