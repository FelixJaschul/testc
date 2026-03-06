# implc

Example repository demonstrating usage of my [wrapper](https://github.com/FelixJaschul/wrapper) library for SDL3/X11-based graphics applications.

## Building

### Prerequisites

- CMake 3.20+
- A C++20 compatible compiler
- Vulkan SDK (for `glslangValidator`) — required for shader compilation

### Build Steps

1. **Clone the repository recursively:**
   ```bash
   git clone --recursive https://github.com/FelixJaschul/implc.git
   cd implc
   ```

2. **Build shadercross (required for GPU examples):**
   ```bash
   ./scripts/build_shadercross.sh
   ```
   This builds the `SDL_shadercross` tool from the submodule, which is needed to convert shaders to Metal (MSL) and DirectX (DXIL) formats.

3. **Configure and build with CMake:**
   ```bash
   cmake -B cmake-build-debug -S .
   cmake --build cmake-build-debug
   ```

   Or use the provided Makefile:
   ```bash
   make
   ```

## Examples

### GPU Examples (SDL_GPU)

#### `sdl_gpu_voxel` — Voxel Raymarching
First-person voxel raymarching demo using SDL_GPU with fragment shaders.

**Controls:**
- `WASD` — Move camera
- `Mouse` — Look around (press `LShift` to release cursor)

**Build:**
```bash
cmake --build cmake-build-debug --target sdl_gpu_voxel
```

#### `sdl_gpu_triangle` — Basic Triangle
Simple colored triangle using SDL_GPU with vertex/fragment shaders.

**Build:**
```bash
cmake --build cmake-build-debug --target sdl_gpu_triangle
```

#### `sdl_gpu_model` — 3D Model Viewer (GPU)
Displays a 3D model (cube.obj) using GPU-accelerated rendering with ImGui interface.

**Controls:**
- `WASD` — Move camera
- `Mouse` — Look around
- `LShift` — Release mouse / Fast movement

**Build:**
```bash
cmake --build cmake-build-debug --target sdl_gpu_model
```

### CPU Examples (SDL_Renderer)

#### `sdl_cpu_triangle` — Basic Triangle
Simple purple triangle using CPU-based software rendering.

**Build:**
```bash
cmake --build cmake-build-debug --target sdl_cpu_triangle
```

#### `sdl_cpu_model` — 3D Model Viewer (CPU)
Displays a 3D model (cube.obj) using software 3D rasterization with depth buffering and lighting.

**Controls:**
- `WASD` — Move camera
- `Mouse` — Look around
- `LShift` — Release mouse / Fast movement

**Build:**
```bash
cmake --build cmake-build-debug --target sdl_cpu_model
```

### X11 Examples (Linux only)

#### `x11_model` — 3D Model Viewer (X11)
Displays a 3D model (cube.obj) using direct X11 rendering without SDL.

**Controls:**
- `WASD` — Move camera
- `Mouse` — Look around
- `LShift` — Release mouse / Fast movement

**Build:**
```bash
cmake --build cmake-build-debug --target x11_model
```

## Project Structure

```
implc/
├── CMakeLists.txt              # Build configuration
├── Makefile                    # Convenience makefile
├── README.md                   # This file
├── scripts/
│   ├── build_shadercross.sh    # Builds SDL_shadercross
│   └── build_shaders.sh        # Compiles GLSL shaders
├── examples/
│   ├── sdl_gpu_voxel/          # Voxel raymarching (GPU)
│   │   ├── sdl_gpu_voxel.c
│   │   ├── voxel_raymarch.vert
│   │   └── voxel_raymarch.frag
│   ├── sdl_gpu_triangle/       # Basic triangle (GPU)
│   │   ├── sdl_gpu_triangle.c
│   │   ├── basic_triangle.vert
│   │   └── basic_triangle.frag
│   ├── sdl_gpu_model/          # 3D model viewer (GPU)
│   │   └── sdl_gpu_model.c
│   ├── sdl_cpu_triangle/       # Basic triangle (CPU)
│   │   └── sdl_cpu_triangle.c
│   ├── sdl_cpu_model/          # 3D model viewer (CPU)
│   │   └── sdl_cpu_model.c
│   └── x11_model/              # 3D model viewer (X11)
│       └── x11_model.c
└── wrapper/                    # Core library (submodule)
    └── core.h                  # Main header with all functionality
```

## Using core.h

The `wrapper/core.h` header provides multiple backends and features:

### Backend Selection

```c
// SDL3 backend with ImGui
#define SDL_IMPLEMENTATION
#define IMGUI_IMPLEMENTATION
#include "core.h"

// SDL3 backend with GPU rendering
#define SDL_IMPLEMENTATION
#define IMGUI_IMPLEMENTATION
#define GPU_IMPLEMENTATION
#include "core.h"

// X11 backend (Linux only)
// (no SDL_IMPLEMENTATION)
#include "core.h"
```

### Features

- **Window Management**: `createWindow()`, `destroyWindow()`, `updateFrame()`
- **Input Handling**: `pollEvents()`, `isKeyDown()`, `isMousePressed()`, `grabMouse()`
- **Camera**: `cameraInit()`, `cameraMove()`, `cameraRotate()`, `cameraGetRay()`
- **3D Models**: `modelCreate()`, `modelLoad()`, `modelTransform()`, `modelUpdate()`
- **Software Rendering**: `renderInit()`, `renderModel()`, `renderScene()`, `renderClear()`
- **GPU Rendering**: `gpuInit()`, `gpuCreatePipeline()`, `gpuRenderFrame()`
- **ImGui**: `imguiInit()`, `imguiNewFrame()`, `imguiEndFrame()`, `imguiRenderDrawData()`
- **Math**: `vec3()`, `add()`, `sub()`, `mul()`, `dot()`, `cross()`, `norm()`

### Example Pattern

```c
#define CORE_IMPLEMENTATION
#define SDL_IMPLEMENTATION
#define IMGUI_IMPLEMENTATION
#define GPU_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#define CAMERA_IMPLEMENTATION
#define MODEL_IMPLEMENTATION
#define RENDER3D_IMPLEMENTATION
#include "core.h"

int main() {
    Window_t win;
    windowInit(&win);
    createWindow(&win);
    
    Camera cam;
    cameraInit(&cam);
    
    Input input;
    inputInit(&input);
    
    Gpu gpu;
    gpuInit(&gpu, &win);
    
    imguiInit(&win, gpu.device, swapchain_format);
    
    // Load models
    Model models[MAX_MODELS];
    int num_models = 0;
    Model* cube = modelCreate(models, &num_models, MAX_MODELS, ...);
    modelLoad(cube, "cube.obj");
    
    // Main loop
    while (running) {
        pollEvents(&win, &input);
        // ... handle input ...
        modelUpdate(models, num_models);
        
        imguiNewFrame();
        // ... build UI ...
        ImGui::Render();
        
        gpuRenderFrame(&gpu, render_callback, &data);
        updateFrame(&win);
    }
    
    destroyWindow(&win);
    return 0;
}
```

## Adding New Examples

1. **Create a directory** under `examples/`:
   ```bash
   mkdir examples/my_example
   ```

2. **Add your source file** (must match directory name):
   ```bash
   # examples/my_example/my_example.c
   ```

3. **Add shaders** (for GPU examples):
   ```bash
   # examples/my_example/my_shader.vert
   # examples/my_example/my_shader.frag
   ```

4. **Update `CMakeLists.txt`**:
   ```cmake
   set(EXAMPLE_DIRS "examples/..." "examples/my_example")
   ```

5. **Update `scripts/build_shaders.sh`** (for GPU examples):
   ```bash
   SHADER_DIRS="... examples/my_example"
   ```

6. **Build**:
   ```bash
   cmake --build cmake-build-debug --target my_example
   ```

## Notes

- **Model Files**: The model examples expect a `cube.obj` file in the same directory. Export from Blender as Wavefront OBJ (triangulated faces).
- **Shader Compilation**: Runs automatically during build if `build_shadercross.sh` was run first.
- **GPU Shaders**: Compiled to SPIR-V, then converted to MSL/DXIL/metallib as available.
- **CPU Rendering**: Uses software rasterization with depth buffering and Phong lighting.
- **X11**: Only available on Linux systems without SDL.
