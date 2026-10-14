# MedievalPort

MedievalPort is a C++ OpenGL application for loading, displaying, and manipulating 3D models in a medieval port scene. It features real-time rendering, drag-and-drop model loading, per-object transformation controls (translate, rotate, scale), and configuration saving/loading. The project uses modern OpenGL, ImGui for GUI controls, and supports textured models with lighting.

> **Note:**  
> - This project is primarily for learning and experimentation, so you may find a variety of coding styles and approaches, and some code is adapted from various sources.
> - The main focus is on building a flexible rendering engine.

## Features

- **Drag-and-drop model loading:** Easily add new `.obj` models to the scene.
- **Multiple model support:** Render and manage multiple models at once.
- **Per-object transformation:** Select objects and translate, rotate, or scale them interactively.
- **Texture mapping:** sRGB diffuse textures, material tint, UV transforms, and alpha masking/blending.
- **Lighting:** One world-space directional sun with Lambert diffuse lighting, ambient fill, and an explicit unlit mode.
- **Configuration saving/loading:** Saves model transforms, file paths, and scene lighting through Save Scene or automatically on exit after edits.
- **Resizable window:** The viewport and projection adjust automatically to window size.
- **ImGui integration:** User-friendly controls for object selection and transformation.
- **OpenGL debug output:** Helpful for development and troubleshooting.

## Getting Started

### Prerequisites

- A C++20 compiler (MSVC, GCC, or Clang)
- CMake 3.24 or later
- Python 3.8 or later (used to generate GLAD; no pip packages are needed)
- An OpenGL 4.5 core-capable driver
- Internet access for the first CMake configure

GLFW, GLAD, ImGui, GLM, nlohmann/json, stb (image loading/resizing), and tinyobjloader
are downloaded automatically. Their source URLs, pinned versions, and SHA-256 checksums
are maintained in [`cmake/Dependencies.cmake`](cmake/Dependencies.cmake). No manual library
installation or Git submodule initialization is required.

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
automatically downloaded GLFW 3.4 source release, built with the selected compiler rather than
a precompiled platform-specific library. Linux source builds require
OpenGL and X11 development packages; the optional Wayland backend can be enabled with
`-DGLFW_BUILD_WAYLAND=ON`. The native macOS OpenGL 4.1 implementation is not supported.

Dependency downloads and generated GLAD files stay under `build/_deps/`, which is ignored by
Git. Subsequent builds reuse them. After the initial download, an offline reconfigure can use
`-DFETCHCONTENT_FULLY_DISCONNECTED=ON`; turn it off again when changing dependency versions.
To provide an existing source checkout instead, set `-DFETCHCONTENT_SOURCE_DIR_<NAME>=<path>`
(for example, `-DFETCHCONTENT_SOURCE_DIR_GLM=/path/to/glm`). Do not commit downloaded sources,
generated headers, libraries, or the legacy `externals/` directory.

Downloads verify both HTTPS certificates and archive checksums. If a toolchain cannot find
its trusted certificate bundle, set `-DCMAKE_TLS_CAINFO=/path/to/ca-bundle.crt` rather than
disabling certificate verification.

In VS Code, the default **build** task configures CMake first, so building or launching the
project also downloads missing dependencies. CMake Tools or `build/compile_commands.json`
provides the downloaded include paths to IntelliSense.

Shaders and skybox textures are copied into `res` beside the executable on each build. Asset
lookup does not depend on the working directory.

### Usage

- **Drag and drop** `.obj` files into the window to load models.
- Use the **ImGui panel** to select and transform models.
- Use **Scene Lighting** to toggle lighting and edit the sun direction, linear RGB color, intensity,
  and ambient strength. These controls also work without any models loaded.
- **Save Scene** persists model and lighting edits. The application also saves authored changes on
  exit and restores the scene on startup. Opening a scene without editing it does not rewrite its file.
- Scene files default to `model_config.json` beside the executable. Use `--scene path/to/model_config.json`
  to open an existing scene saved elsewhere; relative model paths are resolved against that scene's directory.
- Use `--assets path/to/res` to override the asset root and `--load path/to/model.obj` to queue a model at startup.
- Vsync is enabled by default. `--no-vsync` enables uncapped profiling; `--hidden --frames 5` runs a bounded smoke test.
- OBJ parsing, image decoding, and resizing run on a worker. The context thread uploads at most one
  completed model per frame, shares fixed-size texture-array pages, and culls offscreen bounds.

### Lighting

The sun direction specifies the world-space direction light rays travel, not the direction toward
the sun: `(0, -1, 0)` shines down. Nonzero directions are normalized when accepted. Sun color is
linear RGB in `[0, 1]`, intensity ranges from `0` to `4`, and ambient strength ranges from `0` to `1`.
Invalid edits retain the previous valid settings.

**Reset Lighting** restores the defaults: lighting enabled, white sun, direction
`normalize(-1, -1, -1)`, intensity `0.8`, and ambient strength `0.2`. Disabling **Enable Lighting**
restores unlit texture/tint rendering. Lighting changes RGB only; texture alpha, masking, blending,
the skybox, and the UI keep their existing behavior. Zero-scale models use ambient-only lighting
where their remaining geometry is visible.

Scene JSON retains the `model` array and adds a `lighting` object containing `enabled`, `direction`,
`color`, `intensity`, and `ambientStrength`. Existing scenes without `lighting`, missing scene files,
and legacy `null` empty scenes use the default enabled sun. Explicit `enabled: false` is preserved.
Malformed or incomplete explicit lighting is rejected rather than treated as a legacy scene.

This is diffuse/ambient lighting only, without shadows, attenuation, normal maps, specular/PBR
shading, or tone mapping. Unsupported imported material properties continue to produce warnings;
global ambient fill does not implement OBJ ambient/specular parameters such as `Ka`, `Ks`, or `Ns`.

### Tests

```sh
ctest --test-dir build -C Debug --output-on-failure
```

CPU tests cover OBJ validation/indexing, material fallbacks, image decode/resize, scene and lighting
persistence, worker shutdown, camera math, frustum bounds, and robust normal transforms. They can be
built without a graphics driver using `-DMEDIEVALPORT_BUILD_APP=OFF`. Enable live OpenGL
ownership/rendering/drop-path tests with
`-DMEDIEVALPORT_ENABLE_GL_TESTS=ON`; these include directional-lighting and scene-edit regression
tests and require a desktop driver capable of creating a 4.5 core context.
`-DMEDIEVALPORT_WARNINGS_AS_ERRORS=ON` enables the same strict project warning policy used by CI.
CPU-only builds skip GLFW, ImGui, and OpenGL. Scene tests still need Python and GLAD's
generated headers for vertex-layout checks. With both `-DMEDIEVALPORT_BUILD_APP=OFF` and
`-DBUILD_TESTING=OFF`, only GLM, nlohmann/json, stb, and tinyobjloader are downloaded,
and Python is not required.

## Project Structure

- `src/` - Source code
- `res/` - Resources (shaders, textures)
- `build/` - Build output
- `cmake/Dependencies.cmake` - Dependency source URLs, checksums, and build setup
- `README.md` - Project documentation

## License

This project is licensed under the MIT License.

---

**Enjoy exploring and building your medieval port!**
