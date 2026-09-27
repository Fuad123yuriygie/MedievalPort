#include "core/Camera.h"
#include "editor/Editor.h"
#include "graphics/GraphicsContext.h"
#include "graphics/IndexBuffer.h"
#include "graphics/Renderer.h"
#include "graphics/Shader.h"
#include "graphics/TextureArray.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"
#include "graphics/VertexBufferLayout.h"
#include "io/ConfigManager.h"
#include "io/FileParser.h"
#include "io/ImageData.h"
#include "io/PathUtils.h"
#include "ui/ImguiInterface.h"
#include "window/WindowSystem.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <glad/glad.h>
#include <iostream>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace
{
void Check(bool condition, const char* message) {
    if(!condition) {
        throw std::runtime_error(message);
    }
}

void CheckGl() {
    const GLenum error = glGetError();
    Check(error == GL_NO_ERROR, "Unexpected OpenGL error");
}

struct TemporaryDirectory {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("MedievalPort-graphics-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryDirectory() {
        std::filesystem::create_directories(path);
    }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

void Write(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream file(path, std::ios::binary);
    file << contents;
    file.close();
    Check(!file.fail(), "Failed to create graphics test fixture");
}

void TestOwnership(const GraphicsContext& context, const std::filesystem::path& assetRoot) {
    constexpr std::array<float, 9> vertices{-0.5f, -0.5f, 0, 0.5f, -0.5f, 0, 0, 0.5f, 0};
    constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};
    GLuint vertexId = 0;
    GLuint indexId = 0;
    GLuint arrayId = 0;
    GLuint textureId = 0;
    GLuint programId = 0;
    {
        VertexBuffer source(context, std::as_bytes(std::span(vertices)));
        vertexId = source.GetId();
        VertexBuffer moved(std::move(source));
        Check(source.GetId() == 0 && moved.GetId() == vertexId,
              "Vertex buffer move retained ownership");
        VertexBuffer target(context, std::as_bytes(std::span(vertices)));
        const GLuint oldTarget = target.GetId();
        target = std::move(moved);
        Check(moved.GetId() == 0 && glIsBuffer(oldTarget) == GL_FALSE,
              "Vertex buffer move assignment leaked");

        IndexBuffer indexSource(context, indices);
        indexId = indexSource.GetId();
        IndexBuffer indexTarget(context, indices);
        const GLuint oldIndex = indexTarget.GetId();
        indexTarget = std::move(indexSource);
        Check(indexSource.GetId() == 0 && glIsBuffer(oldIndex) == GL_FALSE,
              "Index buffer move assignment leaked");

        VertexArray arraySource(context);
        arrayId = arraySource.GetId();
        VertexArray arrayTarget(context);
        const GLuint oldArray = arrayTarget.GetId();
        arrayTarget = std::move(arraySource);
        Check(arraySource.GetId() == 0 && glIsVertexArray(oldArray) == GL_FALSE,
              "VAO move assignment leaked");
        VertexBufferLayout layout;
        layout.Push<float>(0, 3);
        arrayTarget.SetIndexBuffer(indexTarget);
        arrayTarget.AddBuffer(target, layout);
        GLint attached = 0;
        glGetVertexArrayiv(arrayTarget.GetId(), GL_ELEMENT_ARRAY_BUFFER_BINDING, &attached);
        Check(static_cast<GLuint>(attached) == indexId, "EBO is not explicit VAO state");
        GLint bound = -1;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &bound);
        Check(bound == 0, "DSA mesh construction changed the current VAO");

        TextureArray textureSource(context, 2, 2, 2);
        textureId = textureSource.GetId();
        TextureArray textureTarget(context, 2, 2, 2);
        const GLuint oldTexture = textureTarget.GetId();
        textureTarget = std::move(textureSource);
        Check(textureSource.GetId() == 0 && glIsTexture(oldTexture) == GL_FALSE,
              "Texture move assignment leaked");
        const auto image = MakeSolidImage("test:red", 2, {255, 0, 0, 255});
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, target.GetId());
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 8);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 2);
        textureTarget.UploadLayer(1, image);
        GLint unpackBuffer = 0;
        GLint rowLength = 0;
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
        Check(static_cast<GLuint>(unpackBuffer) == target.GetId() && rowLength == 8,
              "Texture upload did not restore pixel unpack state");
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        std::array<unsigned char, 16> pixels{};
        glGetTextureSubImage(textureTarget.GetId(),
                             0,
                             0,
                             0,
                             1,
                             2,
                             2,
                             1,
                             GL_RGBA,
                             GL_UNSIGNED_BYTE,
                             static_cast<GLsizei>(pixels.size()),
                             pixels.data());
        Check(pixels[0] == 255 && pixels[1] == 0 && pixels[3] == 255,
              "RGBA layer upload is incorrect");
        bool invalidSliceRejected = false;
        try {
            textureTarget.UploadLayer(2, image);
        } catch(const std::invalid_argument&) {
            invalidSliceRejected = true;
        }
        Check(invalidSliceRejected, "Out-of-range texture slice was uploaded");
        glActiveTexture(GL_TEXTURE3);
        textureTarget.Bind(0);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &bound);
        Check(static_cast<GLuint>(bound) == textureId, "Texture binding relied on the active unit");

        Shader programSource(context, assetRoot / "shaders" / "Basic");
        programId = programSource.GetId();
        Shader programTarget(context, assetRoot / "shaders" / "Basic");
        const GLuint oldProgram = programTarget.GetId();
        programTarget = std::move(programSource);
        Check(programSource.GetId() == 0 && glIsProgram(oldProgram) == GL_FALSE,
              "Shader move assignment leaked");
        Check(programTarget.GetUniformLocation("optimized_out") == -1,
              "Missing uniform should be harmless");
        programTarget.SetUniform1i(-1, 0);
        CheckGl();
    }
    Check(glIsBuffer(vertexId) == GL_FALSE && glIsBuffer(indexId) == GL_FALSE &&
              glIsVertexArray(arrayId) == GL_FALSE && glIsTexture(textureId) == GL_FALSE &&
              glIsProgram(programId) == GL_FALSE,
          "GL resources survived their owning scope");

    std::atomic<bool> wrongThreadRejected{false};
    std::thread worker([&] {
        try {
            VertexBuffer invalid(context, std::as_bytes(std::span(vertices)));
        } catch(const std::logic_error&) {
            wrongThreadRejected = true;
        }
    });
    worker.join();
    Check(wrongThreadRejected, "GL construction was allowed without the owning context thread");
    CheckGl();
}

void TestShaderFailures(const GraphicsContext& context, const std::filesystem::path& root) {
    const auto fails =
        [&](const char* name, const std::string& vertex, const std::string& fragment) {
            const auto directory = root / name;
            std::filesystem::create_directories(directory);
            if(!vertex.empty()) {
                Write(directory / (std::string(name) + ".vert"), vertex);
            }
            if(!fragment.empty()) {
                Write(directory / (std::string(name) + ".frag"), fragment);
            }
            bool rejected = false;
            try {
                Shader invalid(context, directory);
            } catch(const std::runtime_error&) {
                rejected = true;
            }
            Check(rejected, "Invalid shader program was accepted");
        };
    fails("Empty", {}, {});
    fails("CompileFailure",
          "#version 450 core\nnot valid GLSL\n",
          "#version 450 core\nout vec4 color; void main(){color=vec4(1);}\n");
    fails("LinkFailure",
          "#version 450 core\nout vec3 data; void main(){data=vec3(1);gl_Position=vec4(0);}\n",
          "#version 450 core\nin vec2 data; out vec4 color; void main(){color=vec4(data,0,1);}\n");
    CheckGl();
}

void TestTexturePages(const GraphicsContext& context, const std::filesystem::path& root) {
    const auto directory = root / "materials";
    std::filesystem::create_directories(directory);
    Write(directory / "red.ppm", std::string("P6\n1 1\n255\n") + std::string("\xff\0\0", 3));
    Write(directory / "green.ppm", std::string("P6\n1 1\n255\n") + std::string("\0\xff\0", 3));
    Write(directory / "blue.ppm", std::string("P6\n1 1\n255\n") + std::string("\0\0\xff", 3));
    Write(directory / "colors.mtl",
          "newmtl red\nmap_Kd red.ppm\nnewmtl green\nmap_Kd green.ppm\n"
          "newmtl blue\nmap_Kd blue.ppm\n");
    const auto modelPath = directory / "colors.obj";
    Write(modelPath,
          "mtllib colors.mtl\nv -0.5 -0.5 0\nv 0.5 -0.5 0\nv 0 0.5 0\n"
          "usemtl red\nf 1 2 3\nusemtl green\nf 1 2 3\nusemtl blue\nf 1 2 3\n");
    ImportSettings settings;
    settings.textureSize = 8;
    settings.textureLayersPerPage = 2;
    FileParser assets(context, root / "pages.json", settings);
    assets.LoadSavedFiles();
    Check(assets.LoadAsset(modelPath) && assets.LoadAsset(modelPath),
          "Textured models could not be queued");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    std::vector<DrawItem> draws;
    while(std::chrono::steady_clock::now() < deadline) {
        assets.ProcessPendingModels();
        draws = assets.BuildDrawList(glm::mat4(1.0f));
        if(draws.size() == 6) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Check(draws.size() == 6, "Textured submesh import timed out");
    std::set<GLuint> pages;
    std::set<std::uint32_t> colors;
    for(const auto& draw : draws) {
        pages.insert(draw.textureArray->GetId());
        Check(draw.textureLayer < 2, "Global texture layer escaped its page");
        std::array<unsigned char, 4> pixel{};
        glGetTextureSubImage(draw.textureArray->GetId(),
                             0,
                             0,
                             0,
                             static_cast<GLint>(draw.textureLayer),
                             1,
                             1,
                             1,
                             GL_RGBA,
                             GL_UNSIGNED_BYTE,
                             static_cast<GLsizei>(pixel.size()),
                             pixel.data());
        colors.insert((static_cast<std::uint32_t>(pixel[0]) << 16) |
                      (static_cast<std::uint32_t>(pixel[1]) << 8) | pixel[2]);
    }
    Check(pages.size() == 2, "Materials were not shared across bounded texture pages");
    Check(colors == std::set<std::uint32_t>{0xff0000, 0x00ff00, 0x0000ff},
          "Local material IDs were not remapped to the correct shared texture layers");
    assets.Shutdown();
    for(const GLuint page : pages) {
        Check(glIsTexture(page) == GL_FALSE, "Shared texture page was leaked");
    }
    CheckGl();
}

void TestRestoreFailure(const GraphicsContext& context, const std::filesystem::path& root) {
    const auto scenePath = root / "restore-failure.json";
    std::array<ModelDescription, 2> original;
    original[0].filePath = PathToUtf8(root / std::filesystem::path(u8"caf\u00e9-\u6a21\u578b.OBJ"));
    original[1].filePath = PathToUtf8(root / "unsupported.fbx");
    const ConfigManager config(scenePath);
    Check(config.SaveObject(original), "Could not prepare transactional restore test");
    bool rejected = false;
    try {
        FileParser assets(context, scenePath);
        assets.LoadSavedFiles();
    } catch(const std::runtime_error&) {
        rejected = true;
    }
    Check(rejected, "An incompletely registered scene was accepted");
    const auto saved = config.LoadObjectFromJson();
    Check(saved.size() == original.size() && saved[1].filePath == original[1].filePath,
          "Failed restoration overwrote the saved scene with a partial prefix");
    CheckGl();
}

void TestScene(WindowSystem& window, const std::filesystem::path& root,
               const std::filesystem::path& assetRoot) {
    const auto modelPath = root / std::filesystem::path(u8"caf\u00e9-\u6a21\u578b.OBJ");
    Write(modelPath, "v -0.5 -0.5 0\nv 0.5 -0.5 0\nv 0.5 0.5 0\nv -0.5 0.5 0\nf 1 2 3 4\n");
    ImportSettings importSettings;
    importSettings.textureSize = 8;
    importSettings.textureLayersPerPage = 2;
    importSettings.uploadsPerFrame = 1;
    const auto scenePath = root / "scene.json";
    FileParser assets(window.GetContext(), scenePath, importSettings);
    assets.LoadSavedFiles();
    Check(assets.LoadAsset(modelPath) && assets.LoadAsset(modelPath), "Async model request failed");
    const auto initial = assets.GetModelViews();
    const ModelId firstId = initial[0].id;
    const ModelId secondId = initial[1].id;
    auto edited = *assets.GetDescription(secondId);
    edited.position.x = 0.1f;
    Check(assets.UpdateTransform(secondId, edited), "Pending model transform edit failed");

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    std::size_t previouslyLoaded = 0;
    while(previouslyLoaded < 2 && std::chrono::steady_clock::now() < deadline) {
        assets.ProcessPendingModels();
        const auto views = assets.GetModelViews();
        const auto count = static_cast<std::size_t>(
            std::count_if(views.begin(), views.end(), [](const auto& model) {
                return model.loaded;
            }));
        Check(count <= previouslyLoaded + 1, "Per-frame GPU upload budget was exceeded");
        for(const auto& view : views) {
            Check(view.error.empty(), "Async model import failed");
        }
        previouslyLoaded = count;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Check(previouslyLoaded == 2, "Async model import timed out");
    Check(assets.GetDescription(secondId)->position.x == 0.1f,
          "Upload overwrote a pending transform edit");
    auto draws = assets.BuildDrawList(glm::mat4(1.0f));
    Check(draws.size() == 2 && draws[0].textureArray == draws[1].textureArray &&
              draws[0].textureLayer == draws[1].textureLayer,
          "Models did not share the default texture layer");
    const GLuint textureId = draws.front().textureArray->GetId();

    edited.position.x = 100;
    Check(assets.UpdateTransform(secondId, edited), "Transform update failed");
    Check(assets.BuildDrawList(glm::mat4(1.0f)).size() == 1,
          "Scene did not cull offscreen geometry");
    edited.position.x = 0.1f;
    edited.scale.x = -1;
    assets.UpdateTransform(secondId, edited);
    draws = assets.BuildDrawList(glm::mat4(1.0f));
    Check(std::any_of(draws.begin(), draws.end(), [](const auto& draw) { return draw.mirrored; }),
          "Mirrored geometry did not select the opposite front-face winding");

    RenderSettings renderSettings;
    renderSettings.samples = 0;
    ImguiInterface gui(window.GetWindow());
    Renderer renderer(window.GetContext(), assetRoot, renderSettings);
    renderer.BeginScene(64, 64);
    renderer.DrawScene(draws);
    std::array<unsigned char, 4> center{};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, center.data());
    Check(center[0] > 200 && center[1] > 200 && center[2] > 200,
          "The no-material model did not render its fallback texture");
    Camera camera;
    camera.Resize(64, 64);
    renderer.DrawSkybox(camera.GetView(), camera.GetProjection());
    GLboolean depthWrites = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrites);
    Check(depthWrites == GL_FALSE, "Skybox pass writes depth");
    renderer.BeginUI();
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrites);
    Check(depthWrites == GL_TRUE, "Pass transition did not restore depth writes");
    CheckGl();

    // Exercise the actual GLFW drop event path and the editor frame orchestration.
    const auto drop = glfwSetDropCallback(window.GetWindow(), nullptr);
    Check(drop != nullptr, "Window has no file drop event callback");
    const std::string droppedPath = PathToUtf8(modelPath);
    const char* paths[] = {droppedPath.c_str()};
    drop(window.GetWindow(), 1, paths);
    glfwSetDropCallback(window.GetWindow(), drop);
    Editor editor(window, renderer, gui, assets);
    editor.Run(4);
    Check(assets.GetModelViews().size() == 3,
          "Drag-and-drop did not reach the async asset pipeline");
    Check(assets.RemoveModel(firstId) && !assets.GetDescription(firstId),
          "Stable-ID model removal failed");
    Check(assets.GetDescription(secondId).has_value(),
          "Removing a model invalidated another model ID");
    Check(initial[0].description.filePath == PathToUtf8(modelPath),
          "An owned model-name snapshot was invalidated");
    Check(assets.SaveScene(), "Scene save reported failure");
    assets.Shutdown();
    assets.Shutdown();
    Check(glIsTexture(textureId) == GL_FALSE, "Scene texture pages outlived shutdown");
    const auto saved = ConfigManager(scenePath).LoadObjectFromJson();
    Check(saved.size() == 2 && saved[0].scale.x == -1,
          "Shutdown lost pending models or edited transforms");
    CheckGl();
}
} // namespace

int main(int argc, char** argv) {
    try {
        Check(argc == 2, "GraphicsTests requires an asset root");
        TemporaryDirectory temporary;
        RenderSettings settings;
        settings.width = 64;
        settings.height = 64;
        settings.samples = 0;
        settings.vsync = false;
        settings.visible = false;
        WindowSystem window(settings);
        TestOwnership(window.GetContext(), argv[1]);
        TestShaderFailures(window.GetContext(), temporary.path);
        TestTexturePages(window.GetContext(), temporary.path);
        TestScene(window, temporary.path, argv[1]);
        TestRestoreFailure(window.GetContext(), temporary.path);
        std::cout
            << "OpenGL ownership, shaders, uploads, rendering, drop, and shutdown tests passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
