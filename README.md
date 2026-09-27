# MedievalPort

MedievalPort is a C++ OpenGL application for loading, displaying, and manipulating 3D models in a medieval port scene. It features real-time rendering, drag-and-drop model loading, per-object transformation controls (translate, rotate, scale), and configuration saving/loading. The project uses modern OpenGL, ImGui for GUI controls, and supports textured models with lighting.

> **Note:**  
> - This project is primarily for learning and experimentation, so you may find a variety of coding styles and approaches, and some code is adapted from various sources.
> - The main focus is on building a flexible rendering engine.
> - Full-featured texturing is a planned enhancement and will be integrated in future updates.

## Features

- **Drag-and-drop model loading:** Easily add new `.obj` models to the scene.
- **Multiple model support:** Render and manage multiple models at once.
- **Per-object transformation:** Select objects and translate, rotate, or scale them interactively.
- **Texture mapping:** Load and display textured models.
- **Lighting:** Realistic lighting with attenuation and normal mapping.
- **Configuration saving/loading:** Automatically saves and restores model positions, rotations, scales, and file paths.
- **Resizable window:** The viewport and projection adjust automatically to window size.
- **ImGui integration:** User-friendly controls for object selection and transformation.
- **OpenGL debug output:** Helpful for development and troubleshooting.

## Getting Started

### Prerequisites

- A C++20 compiler (MSVC, GCC, or Clang)
- CMake 3.24 or later
- An OpenGL 4.5 core-capable driver
- [GLFW](https://www.glfw.org/)
- [GLAD](https://glad.dav1d.de/)
- [ImGui](https://github.com/ocornut/imgui)
- [stb_image](https://github.com/nothings/stb)
- [GLM](https://github.com/g-truc/glm)
- [nlohmann/json](https://github.com/nlohmann/json)

### Building

1. Clone the repository:
    ```sh
    git clone https://github.com/Fuad123yuriygie/MedievalPort
    cd MedievalPort
    ```
2. Configure and build with CMake:
    ```sh
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --config Debug
    ```
3. Run the executable:
    ```sh
    ./build/MedievalPort
    ```

For Visual Studio generators, the executable is `build/Debug/MedievalPort.exe`. CMake uses an
installed GLFW 3.4 package, the bundled x64 MinGW library, or fetches GLFW 3.4 sources for the
selected compiler. Source fetching requires Git and network access. Linux source builds require
OpenGL and X11 development packages; the optional Wayland backend can be enabled with
`-DGLFW_BUILD_WAYLAND=ON`. The native macOS OpenGL 4.1 implementation is not supported.

Shaders and skybox textures are copied into `res` beside the executable on each build. Asset
lookup does not depend on the working directory.

### Usage

- **Drag and drop** `.obj` files into the window to load models.
- Use the **ImGui panel** to select and transform models.
- The application automatically saves your scene configuration on exit and restores it on startup.
- Scene files default to `model_config.json` beside the executable. Use `--scene path/to/model_config.json`
  to open an existing scene saved elsewhere; relative model paths are resolved against that scene's directory.
- Use `--assets path/to/res` to override the asset root and `--load path/to/model.obj` to queue a model at startup.
- Vsync is enabled by default. `--no-vsync` enables uncapped profiling; `--hidden --frames 5` runs a bounded smoke test.
- OBJ parsing, image decoding, and resizing run on a worker. The context thread uploads at most one
  completed model per frame, shares fixed-size texture-array pages, and culls offscreen bounds.

### Tests

```sh
ctest --test-dir build -C Debug --output-on-failure
```

CPU tests cover OBJ validation/indexing, material fallbacks, image decode/resize, persistence,
worker shutdown, camera math, and frustum bounds. They can be built without graphics dependencies
using `-DMEDIEVALPORT_BUILD_APP=OFF`. Enable live OpenGL ownership/rendering/drop-path tests with
`-DMEDIEVALPORT_ENABLE_GL_TESTS=ON`; these require a desktop driver capable of creating a 4.5 core context.
`-DMEDIEVALPORT_WARNINGS_AS_ERRORS=ON` enables the same strict project warning policy used by CI.

## Project Structure

- `src/` - Source code
- `res/` - Resources (shaders, textures)
- `build/` - Build output
- `README.md` - Project documentation

## License

This project is licensed under the MIT License.

---

**Enjoy exploring and building your medieval port!**
