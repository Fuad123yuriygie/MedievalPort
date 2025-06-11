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

- C++17 or later
- CMake
- OpenGL 3.3+
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
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ```
3. Run the executable:
    ```sh
    ./MedievalPort
    ```

### Usage

- **Drag and drop** `.obj` files into the window to load models.
- Use the **ImGui panel** to select and transform models.
- The application automatically saves your scene configuration on exit and restores it on startup.

## Project Structure

- `src/` - Source code
- `res/` - Resources (shaders, textures)
- `build/` - Build output
- `README.md` - Project documentation

## License

This project is licensed under the MIT License.

---

**Enjoy exploring and building your medieval port!**