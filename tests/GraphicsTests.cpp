#include "core/Camera.h"
#include "core/SceneMath.h"
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
#include <cmath>
#include <filesystem>
#include <fstream>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <iterator>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Check(file.is_open(), "Failed to open graphics test fixture");
    const std::string contents{std::istreambuf_iterator<char>(file),
                               std::istreambuf_iterator<char>()};
    Check(!file.bad(), "Failed to read graphics test fixture");
    return contents;
}

void WaitForModels(FileParser& assets, std::size_t expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while(std::chrono::steady_clock::now() < deadline) {
        assets.ProcessPendingModels();
        const auto& views = assets.GetModelViews();
        for(const auto& view : views) {
            Check(view.error.empty(), "Lighting fixture import failed");
        }
        if(views.size() == expected &&
           std::all_of(views.begin(), views.end(), [](const auto& view) { return view.loaded; })) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Check(false, "Lighting fixture import timed out");
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

void TestShaderFailures(const GraphicsContext& context, const std::filesystem::path& root,
                        const std::filesystem::path& assetRoot) {
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
    // Required interfaces are validated, while genuinely optional uniforms remain harmless.
    Shader program(context, assetRoot / "shaders" / "Basic");
    Check(program.GetUniformLocation("optimized_out") == -1, "Missing uniform should be harmless");
    bool missingRejected = false;
    try {
        program.RequireUniformLocation("optimized_out", GL_FLOAT_MAT4);
    } catch(const std::runtime_error&) {
        missingRejected = true;
    }
    Check(missingRejected, "A missing required uniform was accepted");
    bool wrongTypeRejected = false;
    try {
        program.RequireUniformLocation("u_MVP", GL_INT);
    } catch(const std::runtime_error&) {
        wrongTypeRejected = true;
    }
    Check(wrongTypeRejected, "A wrongly typed required uniform was accepted");
    Check(program.RequireUniformLocation("u_MVP", GL_FLOAT_MAT4) >= 0,
          "A valid required uniform was rejected");
    const std::array<std::pair<const char*, GLenum>, 6> lightingUniforms{{
        {"u_NormalMatrix", GL_FLOAT_MAT3},
        {"u_LightingEnabled", GL_BOOL},
        {"u_SunDirection", GL_FLOAT_VEC3},
        {"u_SunColor", GL_FLOAT_VEC3},
        {"u_SunIntensity", GL_FLOAT},
        {"u_AmbientStrength", GL_FLOAT}}};
    for(const auto& [name, type] : lightingUniforms) {
        Check(program.RequireUniformLocation(name, type) >= 0,
              "A required lighting uniform has the wrong type or is absent");
        bool rejected = false;
        try {
            program.RequireUniformLocation(name, GL_FLOAT_MAT4);
        } catch(const std::runtime_error&) {
            rejected = true;
        }
        Check(rejected, "Lighting uniform type validation accepted mat4 for a non-mat4 uniform");
    }

    // DSA setters must work without a bound program, preserve column order, and ignore -1.
    glUseProgram(0);
    const glm::mat3 normalMatrix(glm::vec3(1.0f, 2.0f, 3.0f),
                                 glm::vec3(4.0f, 5.0f, 6.0f),
                                 glm::vec3(7.0f, 8.0f, 9.0f));
    const auto normalLocation = program.RequireUniformLocation("u_NormalMatrix", GL_FLOAT_MAT3);
    program.SetUniformMat3f(normalLocation, normalMatrix);
    std::array<float, 9> uploadedMatrix{};
    glGetUniformfv(program.GetId(), normalLocation, uploadedMatrix.data());
    for(int column = 0; column < 3; ++column) {
        for(int row = 0; row < 3; ++row) {
            Check(uploadedMatrix[static_cast<std::size_t>(column * 3 + row)] ==
                      normalMatrix[column][row],
                  "mat3 setter transposed or lost normal-matrix values");
        }
    }
    const glm::vec3 direction(0.25f, -0.5f, 0.75f);
    const auto directionLocation = program.RequireUniformLocation("u_SunDirection", GL_FLOAT_VEC3);
    program.SetUniform3f(directionLocation, direction);
    std::array<float, 3> uploadedDirection{};
    glGetUniformfv(program.GetId(), directionLocation, uploadedDirection.data());
    for(int axis = 0; axis < 3; ++axis) {
        Check(uploadedDirection[static_cast<std::size_t>(axis)] == direction[axis],
              "vec3 setter lost lighting values");
    }
    program.SetUniformMat3f(-1, normalMatrix);
    program.SetUniform3f(-1, direction);
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
    // The mip chain must be allocated and selected for minification, not just level zero.
    for(const auto& draw : draws) {
        GLint minFilter = 0;
        GLint levelOneWidth = 0;
        glGetTextureParameteriv(draw.textureArray->GetId(), GL_TEXTURE_MIN_FILTER, &minFilter);
        glGetTextureLevelParameteriv(draw.textureArray->GetId(), 1, GL_TEXTURE_WIDTH,
                                     &levelOneWidth);
        Check(minFilter == GL_LINEAR_MIPMAP_LINEAR,
              "World textures do not use mipmapped minification");
        Check(levelOneWidth == 4, "World textures have no allocated mip level 1");
    }
    assets.Shutdown();
    for(const GLuint page : pages) {
        Check(glIsTexture(page) == GL_FALSE, "Shared texture page was leaked");
    }

    const auto reimportPath = directory / "reimport.obj";
    const std::string geometry =
        "mtllib colors.mtl\nv -0.5 -0.5 0\nv 0.5 -0.5 0\nv 0 0.5 0\n";
    Write(reimportPath, geometry + "usemtl red\nf 1 2 3\n");
    settings.textureLayersPerPage = 1;
    FileParser reimported(context, root / "reimport.json", settings);
    reimported.LoadSavedFiles();
    Check(reimported.LoadAsset(reimportPath) && reimported.LoadAsset(reimportPath),
          "Reimport fixture could not queue the old instances");
    WaitForModels(reimported, 2);
    const ModelId firstOld = reimported.GetModelViews()[0].id;
    const ModelId lastOld = reimported.GetModelViews()[1].id;
    draws = reimported.BuildDrawList(glm::mat4(1.0f));
    Check(draws.size() == 2 && draws[0].textureArray == draws[1].textureArray &&
              draws[0].vertexArray == draws[1].vertexArray,
          "Old instances did not share their cached mesh and texture");
    const GLuint oldTexture = draws.front().textureArray->GetId();
    const GLuint oldArray = draws.front().vertexArray->GetId();
    const auto checkColor = [](GLuint texture, const std::array<unsigned char, 4>& expected) {
        std::array<unsigned char, 4> pixel{};
        glGetTextureSubImage(texture, 0, 0, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                             static_cast<GLsizei>(pixel.size()), pixel.data());
        Check(pixel == expected, "Reimport changed an instance's texture color");
    };
    checkColor(oldTexture, {255, 0, 0, 255});
    const auto originalSize = std::filesystem::file_size(reimportPath);
    Write(reimportPath, geometry + "usemtl blue\nf 1 2 3\n# replacement version\n");
    Check(std::filesystem::file_size(reimportPath) != originalSize,
          "Reimport fixture did not change the asset fingerprint");
    Check(reimported.LoadAsset(reimportPath), "Changed OBJ could not be queued again");
    // Check scalar IDs before rebuilding or dereferencing any borrowed draw pointers.
    Check(glIsTexture(oldTexture) == GL_TRUE,
          "Reimport released a texture still referenced by old instances");
    Check(glIsVertexArray(oldArray) == GL_TRUE, "Reimport released the old instances' mesh");
    checkColor(oldTexture, {255, 0, 0, 255});
    draws = reimported.BuildDrawList(glm::mat4(1.0f));
    Check(draws.size() == 2, "Old instances disappeared while their replacement was loading");
    WaitForModels(reimported, 3);
    draws = reimported.BuildDrawList(glm::mat4(1.0f));
    Check(draws.size() == 3, "Replacement did not coexist with both old instances");
    const auto replacement = std::find_if(draws.begin(), draws.end(), [oldArray](const auto& draw) {
        return draw.vertexArray->GetId() != oldArray;
    });
    Check(replacement != draws.end(), "Reimport reused the superseded mesh");
    const GLuint newTexture = replacement->textureArray->GetId();
    const GLuint newArray = replacement->vertexArray->GetId();
    Check(newTexture != oldTexture &&
              std::count_if(draws.begin(), draws.end(), [oldTexture](const auto& draw) {
                  return draw.textureArray->GetId() == oldTexture;
              }) == 2,
          "Replacement changed the texture of an already-loaded instance");
    checkColor(oldTexture, {255, 0, 0, 255});
    checkColor(newTexture, {0, 0, 255, 255});

    auto oldDescription = *reimported.GetDescription(lastOld);
    oldDescription.position.x = 0.1f;
    Check(reimported.UpdateTransform(lastOld, oldDescription),
          "Could not edit an instance of the retired mesh");
    draws = reimported.BuildDrawList(glm::mat4(1.0f));
    Check(draws.size() == 3 &&
              std::count_if(draws.begin(), draws.end(), [oldArray](const auto& draw) {
                  return draw.vertexArray->GetId() == oldArray;
              }) == 2,
          "Transform editing replaced an already-loaded instance's mesh version");

    Check(reimported.RemoveModel(firstOld), "Could not remove the first old instance");
    Check(glIsTexture(oldTexture) == GL_TRUE && glIsVertexArray(oldArray) == GL_TRUE,
          "Removing one old instance reclaimed a mesh still in use");
    draws = reimported.BuildDrawList(glm::mat4(1.0f));
    Check(draws.size() == 2, "Removing one old instance invalidated another instance");
    checkColor(oldTexture, {255, 0, 0, 255});
    Check(reimported.RemoveModel(lastOld), "Could not remove the last old instance");
    Check(glIsTexture(oldTexture) == GL_FALSE && glIsVertexArray(oldArray) == GL_FALSE,
          "Last old instance removal retained its retired mesh or texture page");
    Check(glIsTexture(newTexture) == GL_TRUE && glIsVertexArray(newArray) == GL_TRUE,
          "Retired mesh collection invalidated the current mesh");
    draws = reimported.BuildDrawList(glm::mat4(1.0f));
    Check(draws.size() == 1 && draws.front().textureArray->GetId() == newTexture &&
              draws.front().vertexArray->GetId() == newArray,
          "The replacement did not remain renderable after old instance removal");
    checkColor(newTexture, {0, 0, 255, 255});
    reimported.Shutdown();
    Check(glIsTexture(newTexture) == GL_FALSE && glIsVertexArray(newArray) == GL_FALSE,
          "Reimported resources survived shutdown");
    CheckGl();
}

void TestLighting(const GraphicsContext& context, const std::filesystem::path& assetRoot) {
    const std::array<glm::vec3, 6> normals{{{0.0f, 0.0f, 1.0f},
                                          {0.0f, 0.0f, -1.0f},
                                          {1.0f, 0.0f, 1.0f},
                                          {0.0f, 0.0f, 0.0f},
                                          {1.0e-30f, 0.0f, 1.0e-30f},
                                          {1.0e30f, 0.0f, 1.0e30f}}};
    const std::array<glm::vec2, 4> corners{{{-0.6f, -0.6f},
                                           {0.6f, -0.6f},
                                           {0.6f, 0.6f},
                                           {-0.6f, 0.6f}}};
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    for(std::size_t variant = 0; variant < normals.size(); ++variant) {
        const auto first = static_cast<std::uint32_t>(vertices.size());
        for(const auto& corner : corners) {
            // Variant 2 is an actual tilted plane, with local normal parallel to (1, 0, 1).
            vertices.push_back({{corner.x, corner.y, variant == 2 ? -corner.x : 0.0f},
                                normals[variant], {0.5f, 0.5f}});
        }
        for(const std::uint32_t index : std::array<std::uint32_t, 6>{0, 1, 2, 0, 2, 3}) {
            indices.push_back(first + index);
        }
    }
    VertexBuffer vertexBuffer(context, std::as_bytes(std::span(vertices)));
    IndexBuffer indexBuffer(context, indices);
    VertexArray vertexArray(context);
    VertexBufferLayout layout;
    layout.Push<float>(0, 3);
    layout.Push<float>(1, 3);
    layout.Push<float>(2, 2);
    vertexArray.AddBuffer(vertexBuffer, layout);
    vertexArray.SetIndexBuffer(indexBuffer);
    TextureArray texture(context, 1, 1, 3, true);
    texture.UploadLayer(0, MakeSolidImage("lighting:white", 1, {255, 255, 255, 255}));
    texture.UploadLayer(1, MakeSolidImage("lighting:alpha", 1, {255, 255, 255, 128}));
    texture.UploadLayer(2, MakeSolidImage("lighting:srgb", 1, {128, 64, 192, 128}));
    texture.GenerateMipmaps();

    RenderSettings settings;
    settings.samples = 0;
    settings.clearColor = glm::vec4(0.0f);
    Renderer renderer(context, assetRoot, settings);
    // Use known RGBA precision, including alpha, independently of the window pixel format.
    struct LinearTarget {
        GLuint framebuffer = 0;
        std::array<GLuint, 2> buffers{};
        GLboolean ditherEnabled = glIsEnabled(GL_DITHER);
        ~LinearTarget() {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &framebuffer);
            glDeleteRenderbuffers(static_cast<GLsizei>(buffers.size()), buffers.data());
            if(ditherEnabled) {
                glEnable(GL_DITHER);
            }
        }
    } target;
    glCreateFramebuffers(1, &target.framebuffer);
    glCreateRenderbuffers(static_cast<GLsizei>(target.buffers.size()), target.buffers.data());
    glNamedRenderbufferStorage(target.buffers[0], GL_RGBA8, 64, 64);
    glNamedRenderbufferStorage(target.buffers[1], GL_DEPTH_COMPONENT24, 64, 64);
    glNamedFramebufferRenderbuffer(target.framebuffer, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                    target.buffers[0]);
    glNamedFramebufferRenderbuffer(target.framebuffer, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                    target.buffers[1]);
    glNamedFramebufferDrawBuffer(target.framebuffer, GL_COLOR_ATTACHMENT0);
    glNamedFramebufferReadBuffer(target.framebuffer, GL_COLOR_ATTACHMENT0);
    Check(glCheckNamedFramebufferStatus(target.framebuffer, GL_FRAMEBUFFER) ==
              GL_FRAMEBUFFER_COMPLETE,
          "Linear lighting framebuffer is incomplete");
    const auto begin = [&] {
        renderer.BeginScene(64, 64);
        glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
        glDisable(GL_FRAMEBUFFER_SRGB);
        glDisable(GL_DITHER);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    };
    const auto checkPixel = [](const glm::vec4& expected, const char* message,
                               int x = 32, int y = 32) {
        std::array<unsigned char, 4> pixel{};
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        CheckGl();
        for(int channel = 0; channel < 4; ++channel) {
            const auto expectedByte = static_cast<int>(
                std::lround(std::clamp(expected[channel], 0.0f, 1.0f) * 255.0f));
            const auto actual = static_cast<int>(pixel[static_cast<std::size_t>(channel)]);
            // Two bytes cover interpolation, sRGB texture decode and normalized-target rounding.
            if(std::abs(actual - expectedByte) > 2) {
                throw std::runtime_error(std::string(message) + " (channel " +
                                         std::to_string(channel) + ": got " +
                                         std::to_string(actual) + ", expected " +
                                         std::to_string(expectedByte) + ")");
            }
        }
    };
    const auto checkLightingUniforms = [](const SceneLighting& lighting) {
        GLint currentProgram = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
        Check(currentProgram != 0, "Scene submission did not bind its shader");
        const auto program = static_cast<GLuint>(currentProgram);
        GLint enabled = -1;
        glGetUniformiv(program, glGetUniformLocation(program, "u_LightingEnabled"), &enabled);
        Check(enabled == (lighting.enabled ? 1 : 0), "Lighting toggle leaked across submissions");
        const std::array<std::pair<const char*, glm::vec3>, 2> vectors{{
            {"u_SunDirection", lighting.direction}, {"u_SunColor", lighting.color}}};
        for(const auto& [name, expected] : vectors) {
            std::array<float, 3> value{};
            glGetUniformfv(program, glGetUniformLocation(program, name), value.data());
            for(int axis = 0; axis < 3; ++axis) {
                Check(std::abs(value[static_cast<std::size_t>(axis)] - expected[axis]) < 1.0e-6f,
                      "Lighting vector leaked across submissions");
            }
        }
        const std::array<std::pair<const char*, float>, 2> scalars{{
            {"u_SunIntensity", lighting.intensity},
            {"u_AmbientStrength", lighting.ambientStrength}}};
        for(const auto& [name, expected] : scalars) {
            float value = -1.0f;
            glGetUniformfv(program, glGetUniformLocation(program, name), &value);
            Check(std::abs(value - expected) < 1.0e-6f,
                  "Lighting strength leaked across submissions");
        }
        CheckGl();
    };

    DrawItem base;
    base.vertexArray = &vertexArray;
    base.textureArray = &texture;
    base.indexCount = 6;
    base.baseColor = {0.5f, 0.4f, 0.3f, 1.0f};
    // A null first objectKey must still upload both transforms.
    const auto transformed = [&](const ModelDescription& description) {
        auto draw = base;
        const auto model = MakeModelMatrix(description);
        draw.modelViewProjection = model;
        draw.normalMatrix = MakeNormalMatrix(model);
        draw.mirrored = IsMirrored(description.scale);
        return draw;
    };
    const auto shaded = [&](float strength) {
        return glm::vec4(glm::vec3(base.baseColor) * strength, 1.0f);
    };
    const auto render = [&](const DrawItem& draw, const SceneLighting& lighting,
                            const glm::vec4& expected, const char* message) {
        begin();
        renderer.DrawScene(std::span<const DrawItem>(&draw, 1), lighting);
        checkPixel(expected, message);
    };
    SceneLighting sun;
    sun.direction = {0.0f, 0.0f, -1.0f};
    sun.intensity = 0.6f;
    sun.ambientStrength = 0.2f;
    render(base, sun, shaded(0.8f), "Front-facing surface received incorrect sunlight");
    auto back = base;
    back.firstIndex = 6;
    render(back, sun, shaded(0.2f), "Back-facing normal received diffuse sunlight");
    auto behind = sun;
    behind.direction = {0.0f, 0.0f, 1.0f};
    render(base, behind, shaded(0.2f), "Sun direction was not interpreted as ray travel");
    auto ambient = sun;
    ambient.intensity = 0.0f;
    ambient.ambientStrength = 0.35f;
    ambient.color = {0.0f, 0.25f, 1.0f};
    render(base, ambient, shaded(0.35f), "Ambient fill was tinted or depended on diffuse light");
    ambient.ambientStrength = 0.0f;
    render(base, ambient, shaded(0.0f), "Zero lighting strengths did not produce black RGB");
    auto colored = sun;
    colored.color = {0.25f, 0.5f, 1.0f};
    colored.intensity = 1.2f;
    colored.ambientStrength = 0.1f;
    const glm::vec4 coloredPixel(0.2f, 0.28f, 0.39f, 1.0f);
    render(base, colored, coloredPixel, "Sun color or intensity was not applied in linear RGB");

    ModelDescription description;
    description.rotation.y = 60.0f;
    render(transformed(description), sun, shaded(0.5f), "Rotated normal was not lit in world space");
    description.rotation.y = 25.0f;
    description.scale = {1.5f, 0.6f, 0.5f};
    auto nonuniform = transformed(description);
    nonuniform.firstIndex = 12;
    nonuniform.modelViewProjection =
        glm::scale(glm::mat4(1.0f), glm::vec3(0.8f, 0.8f, 0.4f)) *
        nonuniform.modelViewProjection;
    const double angle = glm::radians(25.0);
    const float normalZ = static_cast<float>(
        (2.0 * std::cos(angle) - (2.0 / 3.0) * std::sin(angle)) / std::sqrt(4.0 + 4.0 / 9.0));
    render(nonuniform, sun, shaded(0.2f + 0.6f * normalZ),
           "Nonuniformly scaled tilted surface did not use inverse-transpose normals");

    description = ModelDescription{};
    description.scale.x = -1.0f;
    render(transformed(description), sun, shaded(0.8f),
           "Mirrored surface was culled or its transformed normal was negated twice");
    description.scale.y = -1.0f;
    render(transformed(description), sun, shaded(0.8f), "Even mirror parity changed sunlight");
    description.scale = {1.0f, 1.0f, 0.0f};
    const auto singular = transformed(description);
    Check(singular.normalMatrix == glm::mat3(0.0f), "Singular model retained a valid normal matrix");
    render(singular, sun, shaded(0.2f), "Visible zero-Z-scale plane was not ambient-only");
    auto zeroNormal = base;
    zeroNormal.firstIndex = 18;
    render(zeroNormal, sun, shaded(0.2f), "Zero vertex normal did not fall back to ambient");
    auto tinyNormal = base;
    tinyNormal.firstIndex = 24;
    const auto diagonalPixel = shaded(0.2f + 0.6f / std::sqrt(2.0f));
    render(tinyNormal, sun, diagonalPixel, "Tiny fragment normal underflowed during normalization");
    auto hugeNormal = base;
    hugeNormal.firstIndex = 30;
    render(hugeNormal, sun, diagonalPixel, "Huge fragment normal overflowed during normalization");

    const std::array<int, 2> objectKeys{};
    description = ModelDescription{};
    description.position.x = -0.5f;
    description.scale = {0.65f, 0.8f, 1.0f};
    auto left = transformed(description);
    left.objectKey = &objectKeys[0];
    description.position.x = 0.5f;
    description.rotation.y = 60.0f;
    auto right = transformed(description);
    right.objectKey = &objectKeys[1];
    begin();
    renderer.DrawScene(std::array<DrawItem, 2>{left, right}, sun);
    checkPixel(shaded(0.8f), "First object's normal matrix was stale", 16, 32);
    checkPixel(shaded(0.5f), "Normal matrix was not uploaded when objectKey changed", 48, 32);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.DrawScene(std::array<DrawItem, 2>{right, left}, sun);
    checkPixel(shaded(0.8f), "Reordered objects reused the previous normal matrix", 16, 32);
    checkPixel(shaded(0.5f), "Reordered objects reused the previous MVP", 48, 32);

    auto unlit = colored;
    unlit.enabled = false;
    unlit.direction = {1.0f, 0.0f, 0.0f};
    unlit.intensity = 2.0f;
    unlit.ambientStrength = 0.75f;
    begin();
    renderer.DrawScene(std::span<const DrawItem>(&base, 1), sun);
    checkPixel(shaded(0.8f), "Initial lit submission failed");
    renderer.DrawScene({}, unlit);
    checkLightingUniforms(unlit);
    checkPixel(shaded(0.8f), "Empty disabled submission changed the framebuffer");
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.DrawScene(std::span<const DrawItem>(&base, 1), unlit);
    checkPixel(base.baseColor, "Disabled submission retained lighting from its predecessor");
    checkLightingUniforms(unlit);
    renderer.DrawScene({}, colored);
    checkLightingUniforms(colored);
    checkPixel(base.baseColor, "Empty enabled submission changed the framebuffer");
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.DrawScene(std::span<const DrawItem>(&base, 1), colored);
    checkPixel(coloredPixel, "Re-enabled lighting retained disabled or stale sun settings");
    renderer.DrawScene({}, sun);
    checkLightingUniforms(sun);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.DrawScene(std::span<const DrawItem>(&back, 1), sun);
    checkPixel(shaded(0.2f), "Lighting or object state leaked across empty submissions");

    auto alpha = base;
    alpha.textureLayer = 1;
    alpha.baseColor.a = 0.5f;
    const float coverage = (128.0f / 255.0f) * alpha.baseColor.a;
    alpha.alphaCutoff = 0.3f;
    render(alpha, sun, glm::vec4(0.0f), "Lighting changed alpha-mask rejection");
    render(alpha, unlit, glm::vec4(0.0f), "Disabled lighting changed alpha-mask rejection");
    alpha.alphaCutoff = 0.2f;
    const glm::vec4 litAlpha(glm::vec3(shaded(0.8f)), coverage);
    render(alpha, sun, litAlpha, "Lighting modified texture * tint alpha or masked visible coverage");
    render(alpha, unlit, glm::vec4(glm::vec3(base.baseColor), coverage),
           "Unlit alpha differs from lit alpha");
    // Use exactly representable coverage to test '<', not texture normalization roundoff.
    auto boundary = alpha;
    boundary.textureLayer = 0;
    boundary.alphaCutoff = 0.5f;
    render(boundary, sun, glm::vec4(glm::vec3(shaded(0.8f)), 0.5f),
           "Alpha equal to the cutoff was incorrectly discarded");
    alpha.alphaCutoff = 0.0f;
    alpha.blended = true;
    const glm::vec4 background(0.125f, 0.25f, 0.375f, 0.5f);
    for(const auto& lighting : std::array<SceneLighting, 2>{sun, unlit}) {
        begin();
        glClearColor(background.r, background.g, background.b, background.a);
        glClear(GL_COLOR_BUFFER_BIT);
        renderer.DrawScene(std::span<const DrawItem>(&alpha, 1), lighting);
        const glm::vec4 source(lighting.enabled ? glm::vec3(litAlpha) : glm::vec3(base.baseColor),
                               coverage);
        // Preserve the renderer's existing SRC_ALPHA / ONE_MINUS_SRC_ALPHA policy for RGBA.
        checkPixel(source * coverage + background * (1.0f - coverage),
                   "Lighting changed material blending or the blended alpha channel");
        GLboolean depthWrites = GL_FALSE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrites);
        Check(depthWrites == GL_TRUE && glIsEnabled(GL_BLEND) == GL_FALSE,
              "Lit blend submission leaked blend or depth-write state");
    }

    const auto decodeSrgb = [](float value) {
        value /= 255.0f;
        return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
    };
    auto tinted = base;
    tinted.textureLayer = 2;
    const glm::vec3 decoded(decodeSrgb(128.0f), decodeSrgb(64.0f), decodeSrgb(192.0f));
    render(tinted, sun, glm::vec4(decoded * glm::vec3(base.baseColor) * 0.8f, 128.0f / 255.0f),
           "Lighting did not preserve sRGB texture decode and linear alpha");
    CheckGl();
}

void TestLightingEdits(WindowSystem& window, const std::filesystem::path& root,
                       const std::filesystem::path& assetRoot) {
    const auto scenePath = root / "empty-lighting.json";
    ImportSettings importSettings;
    importSettings.textureSize = 8;
    importSettings.textureLayersPerPage = 2;
    RenderSettings renderSettings;
    renderSettings.samples = 0;
    ImguiInterface gui(window.GetWindow());
    Renderer renderer(window.GetContext(), assetRoot, renderSettings);
    SceneLighting disabled;
    {
        FileParser assets(window.GetContext(), scenePath, importSettings);
        Check(!assets.SaveScene() && !std::filesystem::exists(scenePath),
              "An unrestored scene could overwrite its configuration");
        assets.LoadSavedFiles();
        Check(assets.GetModelViews().empty() && assets.GetLighting() == SceneLighting{} &&
                  !assets.IsDirty(),
              "Missing empty scene did not restore clean default lighting");
        Check(assets.UpdateLighting(SceneLighting{}) && !assets.IsDirty(),
              "Default lighting no-op dirtied an empty scene");
        Editor editor(window, renderer, gui, assets);
        editor.Run(1);
        Check(!assets.IsDirty() && !std::filesystem::exists(scenePath),
              "Opening the empty editor authored or saved lighting");

        auto lighting = assets.GetLighting();
        lighting.direction = {0.0f, -3.0f, -4.0f};
        lighting.color = {0.25f, 0.5f, 0.75f};
        lighting.intensity = 1.25f;
        lighting.ambientStrength = 0.125f;
        Check(assets.UpdateLighting(lighting) && assets.IsDirty(),
              "Lighting-only edit did not dirty an empty scene");
        const auto canonical = assets.GetLighting();
        Check(std::abs(canonical.direction.y + 0.6f) < 1.0e-6f &&
                  std::abs(canonical.direction.z + 0.8f) < 1.0e-6f &&
                  canonical.direction.x == 0.0f,
              "Lighting edit did not retain a normalized ray direction");
        editor.Run(2);
        Check(assets.IsDirty() && assets.GetLighting() == canonical &&
                  assets.GetModelViews().empty(),
              "Empty editor frames lost lighting-only edits");
        Check(assets.SaveScene() && !assets.IsDirty(), "Lighting-only save did not clear dirty");
        const auto saved = ConfigManager(scenePath).LoadScene();
        Check(saved.models.empty() && saved.lighting == canonical,
              "Empty scene lost lighting during serialization");
        const auto originalBytes = ReadText(scenePath);
        Check(assets.UpdateLighting(lighting) && !assets.IsDirty() &&
                  assets.UpdateLighting(canonical) && !assets.IsDirty(),
              "Canonical or equivalent authored lighting no-op dirtied the scene");

        auto badDirection = canonical;
        badDirection.direction = glm::vec3(0.0f);
        auto badColor = canonical;
        badColor.color.g = 1.01f;
        auto badIntensity = canonical;
        badIntensity.intensity = std::numeric_limits<float>::quiet_NaN();
        auto badAmbient = canonical;
        badAmbient.ambientStrength = -0.1f;
        for(const auto& invalid :
            std::array<SceneLighting, 4>{badDirection, badColor, badIntensity, badAmbient}) {
            Check(!assets.UpdateLighting(invalid) && assets.GetLighting() == canonical &&
                      !assets.IsDirty(),
                  "Rejected lighting edit changed clean scene state");
        }
        editor.Run(1);
        Check(ReadText(scenePath) == originalBytes && !assets.IsDirty(),
              "No-op or rejected lighting edits rewrote the scene");

        disabled = canonical;
        disabled.enabled = false;
        Check(assets.UpdateLighting(disabled) && assets.IsDirty(),
              "Disabling lighting did not dirty the scene");
        Check(!assets.UpdateLighting(badDirection) && assets.GetLighting() == disabled &&
                  assets.IsDirty(),
              "Rejected lighting edit cleared an existing dirty change");
        editor.Run(1);
        Check(assets.SaveScene() && !assets.IsDirty(), "Disabled lighting could not be saved");
        assets.Shutdown();
        Check(!assets.SaveScene() && !assets.UpdateLighting(SceneLighting{}),
              "Stopped scene accepted a lighting edit or save");
    }
    const auto disabledBytes = ReadText(scenePath);
    {
        FileParser assets(window.GetContext(), scenePath, importSettings);
        assets.LoadSavedFiles();
        Check(assets.GetLighting() == disabled && !assets.GetLighting().enabled &&
                  !assets.IsDirty() && assets.GetModelViews().empty(),
              "Explicit disabled lighting did not reopen cleanly");
        Editor editor(window, renderer, gui, assets);
        editor.Run(1);
        Check(ReadText(scenePath) == disabledBytes && !assets.IsDirty(),
              "Reopening disabled lighting rewrote the scene");
        Check(assets.UpdateLighting(SceneLighting{}) && assets.GetLighting() == SceneLighting{} &&
                  assets.IsDirty(),
              "Reset lighting did not restore defaults and dirty the scene");
        editor.Run(1);
        Check(assets.SaveScene() && !assets.IsDirty(), "Reset lighting could not be saved");
        Check(assets.UpdateLighting(SceneLighting{}) && !assets.IsDirty(),
              "Repeated lighting reset dirtied an already-default scene");
    }
    const auto resetBytes = ReadText(scenePath);
    {
        FileParser assets(window.GetContext(), scenePath, importSettings);
        assets.LoadSavedFiles();
        Check(assets.GetLighting() == SceneLighting{} && !assets.IsDirty(),
              "Saved lighting reset did not survive reopen");
        Editor editor(window, renderer, gui, assets);
        editor.Run(1);
    }
    Check(ReadText(scenePath) == resetBytes, "Clean reopened editor rewrote reset lighting");
    CheckGl();
}

void TestLightingRestore(const GraphicsContext& context, const std::filesystem::path& root) {
    ImportSettings settings;
    settings.textureSize = 8;
    settings.textureLayersPerPage = 2;
    Write(root / "legacy-lighting.obj", "v -0.5 -0.5 0\nv 0.5 -0.5 0\nv 0 0.5 0\nf 1 2 3\n");
    const std::array<std::string, 4> legacy{{
        "", "null \n", "{ \"model\" : [] }\n",
        R"({"model":[{"filePath":"legacy-lighting.obj","position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}]})"}};
    for(std::size_t index = 0; index < legacy.size(); ++index) {
        const auto scenePath = root / ("legacy-lighting-" + std::to_string(index) + ".json");
        if(!legacy[index].empty()) {
            Write(scenePath, legacy[index]);
        }
        {
            FileParser assets(context, scenePath, settings);
            assets.LoadSavedFiles();
            assets.LoadSavedFiles();
            const std::size_t modelCount = index == 3 ? 1 : 0;
            WaitForModels(assets, modelCount);
            Check(assets.GetLighting() == SceneLighting{} && !assets.IsDirty(),
                  "Legacy restore changed default lighting or dirtied the scene");
            Check(assets.UpdateLighting(SceneLighting{}) && !assets.IsDirty(),
                  "Default lighting no-op dirtied a legacy scene");
            assets.Shutdown();
            Check(!assets.SaveScene(), "Shutdown legacy scene allowed explicit saving");
        }
        if(legacy[index].empty()) {
            Check(!std::filesystem::exists(scenePath), "Opening a missing scene created its file");
        }
        else {
            Check(ReadText(scenePath) == legacy[index],
                  "Opening or shutting down a legacy scene rewrote its bytes");
        }
    }

    // A null document is legacy; an explicitly null, incomplete or invalid lighting object is not.
    const std::array<std::string, 6> malformed{{
        "null",
        R"({"enabled":true})",
        R"({"enabled":1,"direction":[0,0,-1],"color":[1,1,1],"intensity":0.8,"ambientStrength":0.2})",
        R"({"enabled":false,"direction":[0,0,0],"color":[1,1,1],"intensity":0.8,"ambientStrength":0.2})",
        R"({"enabled":true,"direction":[0,0,-1],"color":[1,1.01,1],"intensity":0.8,"ambientStrength":0.2})",
        R"({"enabled":true,"direction":[0,0,-1],"color":[1,1,1],"intensity":"0.8","ambientStrength":0.2})"}};
    for(std::size_t index = 0; index < malformed.size(); ++index) {
        const auto scenePath = root / ("malformed-lighting-" + std::to_string(index) + ".json");
        const std::string original = "{\"model\":[],\"lighting\":" + malformed[index] + "}\n";
        Write(scenePath, original);
        {
            FileParser assets(context, scenePath, settings);
            bool rejected = false;
            try {
                assets.LoadSavedFiles();
            } catch(const std::exception&) {
                rejected = true;
            }
            Check(rejected && !assets.IsDirty() && assets.GetModelViews().empty() &&
                      assets.GetLighting() == SceneLighting{},
                  "Malformed explicit lighting was accepted or partially restored");
            auto lighting = assets.GetLighting();
            lighting.ambientStrength = 0.4f;
            Check(assets.UpdateLighting(lighting) && assets.IsDirty(),
                  "Could not prepare a dirty failed-restore scene");
            Check(!assets.SaveScene() && assets.IsDirty(),
                  "Malformed lighting restore did not guard explicit saving");
            assets.Shutdown();
            Check(assets.IsDirty() && !assets.SaveScene(),
                  "Failed restore shutdown discarded dirty state or enabled saving");
        }
        Check(ReadText(scenePath) == original, "Failed lighting restore mutated the original file");
    }
    CheckGl();
}

void TestLightingSaveFailure(const GraphicsContext& context, const std::filesystem::path& root) {
    const auto scenePath = root / "lighting-save-failure.json";
    const auto preservedPath = root / "lighting-save-preserved.json";
    const ConfigManager config(scenePath);
    Check(config.SaveScene(SceneDocument{}), "Could not prepare lighting save-failure fixture");
    const auto originalBytes = ReadText(scenePath);
    FileParser assets(context, scenePath);
    assets.LoadSavedFiles();
    auto lighting = assets.GetLighting();
    lighting.intensity = 1.5f;
    Check(assets.UpdateLighting(lighting) && assets.IsDirty(),
          "Could not prepare a dirty lighting-only scene");

    // A nonempty directory deterministically blocks atomic replacement on Windows and POSIX,
    // without depending on permissions (which elevated test users can bypass).
    std::filesystem::rename(scenePath, preservedPath);
    std::filesystem::create_directory(scenePath);
    const auto sentinel = scenePath / "original.json";
    Write(sentinel, originalBytes);
    Check(!assets.SaveScene() && assets.IsDirty() && assets.GetLighting() == lighting,
          "Failed lighting save cleared or changed the authored state");
    Check(ReadText(preservedPath) == originalBytes && ReadText(sentinel) == originalBytes,
          "Failed atomic replacement damaged existing scene data");
    for(const auto& entry : std::filesystem::directory_iterator(root)) {
        Check(PathToUtf8(entry.path().filename()).find("lighting-save-failure.json.tmp-") != 0,
              "Failed lighting save leaked a temporary replacement directory");
    }
    std::filesystem::remove(sentinel);
    std::filesystem::remove(scenePath);
    std::filesystem::rename(preservedPath, scenePath);
    Check(assets.SaveScene() && !assets.IsDirty() && config.LoadScene().lighting == lighting,
          "Retrying a failed lighting save did not commit and clear dirty");
    assets.Shutdown();

    const auto missingPath = root / "missing-lighting-parent" / "scene.json";
    FileParser missing(context, missingPath);
    missing.LoadSavedFiles();
    Check(missing.UpdateLighting(lighting) && missing.IsDirty(),
          "Could not prepare missing-parent save failure");
    Check(!missing.SaveScene() && missing.IsDirty(), "Missing-parent save cleared dirty state");
    missing.Shutdown();
    Check(missing.IsDirty() && !missing.SaveScene() && !std::filesystem::exists(missingPath),
          "Failed shutdown save pretended to persist lighting");
    CheckGl();
}

void TestCachedNormalTransforms(const GraphicsContext& context, const std::filesystem::path& root) {
    const auto modelPath = root / "cached-normals.obj";
    Write(modelPath, "v -2 -2 0\nv 2 -2 0\nv 2 2 0\nv -2 2 0\nf 1 2 3 4\n");
    const auto scenePath = root / "cached-normals.json";
    ModelDescription original;
    original.filePath = PathToUtf8(modelPath);
    original.position = {0.1f, -0.1f, 0.0f};
    original.rotation = {15.0f, 25.0f, 10.0f};
    original.scale = {-0.15f, 0.2f, 0.4f};
    SceneDocument document;
    document.models.push_back(original);
    Check(ConfigManager(scenePath).SaveScene(document), "Could not prepare cached-normal fixture");
    ImportSettings settings;
    settings.textureSize = 8;
    settings.textureLayersPerPage = 2;
    FileParser assets(context, scenePath, settings);
    assets.LoadSavedFiles();
    WaitForModels(assets, 1);
    const auto id = assets.GetModelViews().front().id;
    const auto initialDraws = assets.BuildDrawList(glm::mat4(1.0f));
    const auto initialModel = MakeModelMatrix(original);
    Check(initialDraws.size() == 1 && initialDraws.front().modelViewProjection == initialModel &&
              initialDraws.front().normalMatrix == MakeNormalMatrix(initialModel) &&
              initialDraws.front().mirrored && !assets.IsDirty(),
          "Restored model did not cache its normal matrix and signed-scale winding cleanly");
    const auto projection = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.75f, 0.25f));
    const auto projected = assets.BuildDrawList(projection);
    Check(projected.size() == 1 &&
              projected.front().modelViewProjection == projection * initialModel &&
              projected.front().normalMatrix == initialDraws.front().normalMatrix,
          "Camera/projection changes contaminated the cached world normal matrix");
    Check(assets.UpdateTransform(id, original) && !assets.IsDirty(),
          "No-op transform dirtied the scene");

    auto invalid = original;
    invalid.position = {0.4f, 0.2f, 0.0f};
    invalid.rotation = glm::vec3(0.0f);
    invalid.scale = {std::numeric_limits<float>::max(), 0.3f, 1.0f};
    const auto candidateModel = MakeModelMatrix(invalid);
    const auto candidateNormal = MakeNormalMatrix(candidateModel);
    for(int column = 0; column < 4; ++column) {
        Check(IsFinite(glm::vec3(candidateModel[column])),
              "Atomicity fixture must have a valid finite candidate model matrix");
    }
    for(int column = 0; column < 3; ++column) {
        Check(IsFinite(candidateNormal[column]),
              "Atomicity fixture must have a finite candidate normal matrix");
    }
    bool boundsOverflow = false;
    try {
        TransformBounds(Bounds{{-2.0f, -2.0f, 0.0f}, {2.0f, 2.0f, 0.0f}}, candidateModel);
    } catch(const std::invalid_argument&) {
        boundsOverflow = true;
    }
    Check(boundsOverflow, "Atomicity fixture did not overflow transformed bounds");
    const auto checkRejectedTransform = [&] {
        const auto before = *assets.GetDescription(id);
        const bool wasDirty = assets.IsDirty();
        const auto beforeDraws = assets.BuildDrawList(glm::mat4(1.0f));
        Check(beforeDraws.size() == 1, "Atomicity fixture is not visible before the failed edit");
        const auto beforeDraw = beforeDraws.front();
        const std::array<glm::mat4, 4> probes{{
            glm::mat4(1.0f),
            glm::translate(glm::mat4(1.0f), glm::vec3(1.25f, 0.0f, 0.0f)),
            glm::translate(glm::mat4(1.0f), glm::vec3(-1.25f, 0.0f, 0.0f)),
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f))}};
        std::array<std::size_t, 4> visible{};
        for(std::size_t index = 0; index < probes.size(); ++index) {
            visible[index] = assets.BuildDrawList(probes[index]).size();
        }
        Check(!assets.UpdateTransform(id, invalid), "Bounds-overflow transform edit was accepted");
        const auto after = *assets.GetDescription(id);
        const auto afterDraws = assets.BuildDrawList(glm::mat4(1.0f));
        Check(after.filePath == before.filePath && after.position == before.position &&
                  after.rotation == before.rotation && after.scale == before.scale &&
                  assets.IsDirty() == wasDirty,
              "Failed transform edit partially committed its description or dirty state");
        Check(afterDraws.size() == 1 &&
                  afterDraws.front().normalMatrix == beforeDraw.normalMatrix &&
                  afterDraws.front().modelViewProjection == beforeDraw.modelViewProjection &&
                  afterDraws.front().mirrored == beforeDraw.mirrored &&
                  afterDraws.front().distanceSquared == beforeDraw.distanceSquared &&
                  afterDraws.front().objectKey == beforeDraw.objectKey,
              "Failed transform edit partially committed cached draw state or bounds center");
        for(std::size_t index = 0; index < probes.size(); ++index) {
            Check(assets.BuildDrawList(probes[index]).size() == visible[index],
                  "Failed transform edit changed bounds/frustum visibility");
        }
    };
    const auto savedBytes = ReadText(scenePath);
    checkRejectedTransform();
    Check(ReadText(scenePath) == savedBytes, "Rejected transform edit mutated the saved scene");
    auto edited = original;
    edited.rotation.z += 5.0f;
    Check(assets.UpdateTransform(id, edited) && assets.IsDirty(), "Valid transform edit failed");
    const auto editedDraws = assets.BuildDrawList(glm::mat4(1.0f));
    Check(editedDraws.size() == 1 &&
              editedDraws.front().normalMatrix == MakeNormalMatrix(MakeModelMatrix(edited)) &&
              editedDraws.front().normalMatrix != initialDraws.front().normalMatrix,
          "Successful transform edit did not replace the cached normal matrix");
    checkRejectedTransform();
    Check(assets.SaveScene() && !assets.IsDirty(), "Could not save the valid transform edit");

    const std::array<std::pair<glm::vec3, bool>, 4> extremes{{
        {{-1.0e30f, 1.0e30f, 1.0e30f}, true},
        {{-1.0e30f, -1.0e30f, 1.0e30f}, false},
        {{-1.0e-30f, 1.0e-30f, 1.0e-30f}, true},
        {{-1.0e-30f, -1.0e-30f, 1.0e-30f}, false}}};
    for(const auto& [scale, mirrored] : extremes) {
        edited.position = glm::vec3(0.0f);
        edited.rotation = glm::vec3(0.0f);
        edited.scale = scale;
        Check(assets.UpdateTransform(id, edited), "Finite extreme signed scale was rejected");
        // Centered bounds and reciprocal projection keep culling/distance calculations finite.
        const auto viewProjection =
            glm::scale(glm::mat4(1.0f), glm::vec3(1.0f / std::abs(scale.x)));
        const auto draws = assets.BuildDrawList(viewProjection);
        const auto model = MakeModelMatrix(edited);
        Check(draws.size() == 1 && draws.front().mirrored == mirrored &&
                  draws.front().normalMatrix == MakeNormalMatrix(model) &&
                  draws.front().normalMatrix != glm::mat3(0.0f) &&
                  draws.front().modelViewProjection == viewProjection * model &&
                  std::isfinite(draws.front().distanceSquared),
              "Extreme signed scale lost cached normals or used overflowing determinant winding");
        for(int column = 0; column < 3; ++column) {
            Check(IsFinite(draws.front().normalMatrix[column]),
                  "Extreme scale published nonfinite cached normals");
        }
    }
    edited.scale = {0.2f, 0.2f, 0.0f};
    Check(assets.UpdateTransform(id, edited), "Legal zero scale was rejected by the scene owner");
    const auto singular = assets.BuildDrawList(glm::mat4(1.0f));
    Check(singular.size() == 1 && singular.front().normalMatrix == glm::mat3(0.0f) &&
              !singular.front().mirrored,
          "Visible zero-Z-scale instance retained stale normals or winding");
    Check(assets.SaveScene() && !assets.IsDirty(), "Could not save legal zero-scale fixture");
    assets.Shutdown();

    ModelDescription oversized;
    oversized.filePath = PathToUtf8(modelPath);
    oversized.scale.x = std::numeric_limits<float>::max();
    auto healthy = oversized;
    healthy.scale = glm::vec3(0.25f);
    const auto waitForBoundsFailure = [](FileParser& pending) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while(std::chrono::steady_clock::now() < deadline) {
            pending.ProcessPendingModels();
            const auto& views = pending.GetModelViews();
            Check(views.size() == 2 && views[1].error.empty(),
                  "Deferred bounds failure affected the healthy sibling");
            if(!views[0].loaded && !views[0].error.empty() && views[1].loaded) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(false, "Deferred bounds validation did not finish per instance");
    };
    for(const bool correct : {false, true}) {
        const auto restorePath = root / (correct ? "corrected-bounds.json" : "deferred-bounds.json");
        SceneDocument deferred;
        deferred.models = {oversized, healthy};
        const ConfigManager config(restorePath);
        Check(config.SaveScene(deferred), "Could not prepare deferred bounds fixture");
        const auto originalBytes = ReadText(restorePath);
        FileParser restored(context, restorePath, settings);
        restored.LoadSavedFiles();
        const ModelId oversizedId = restored.GetModelViews().front().id;
        waitForBoundsFailure(restored);
        const auto failed = restored.GetModelViews().front();
        Check(failed.description.filePath == oversized.filePath &&
                  failed.description.position == oversized.position &&
                  failed.description.rotation == oversized.rotation &&
                  failed.description.scale == oversized.scale && !restored.IsDirty() &&
                  ReadText(restorePath) == originalBytes,
              "Deferred bounds failure changed the authored description or clean scene");
        auto draws = restored.BuildDrawList(glm::mat4(1.0f));
        Check(draws.size() == 1 &&
                  draws.front().modelViewProjection == MakeModelMatrix(healthy) &&
                  draws.front().normalMatrix == MakeNormalMatrix(MakeModelMatrix(healthy)),
              "The healthy sibling did not retain its renderable transform");
        const GLuint cachedArray = draws.front().vertexArray->GetId();
        const GLuint cachedTexture = draws.front().textureArray->GetId();
        if(correct) {
            auto stillInvalid = oversized;
            stillInvalid.position.x = 0.2f;
            stillInvalid.scale.x = -std::numeric_limits<float>::max();
            Check(!restored.UpdateTransform(oversizedId, stillInvalid),
                  "Unattached instance accepted an edit that overflows the cached mesh bounds");
            const auto unchanged = restored.GetModelViews().front();
            Check(unchanged.description.filePath == failed.description.filePath &&
                      unchanged.description.position == failed.description.position &&
                      unchanged.description.rotation == failed.description.rotation &&
                      unchanged.description.scale == failed.description.scale &&
                      unchanged.error == failed.error && !unchanged.loaded && !restored.IsDirty(),
                  "Rejected deferred edit partially committed the description, error or dirty state");
            draws = restored.BuildDrawList(glm::mat4(1.0f));
            Check(draws.size() == 1 && draws.front().vertexArray->GetId() == cachedArray &&
                      draws.front().modelViewProjection == MakeModelMatrix(healthy),
                  "Rejected deferred edit changed mesh attachment or the healthy sibling");

            // A correction must attach the cached mesh immediately, even without its source file.
            const auto offlinePath = root / "cached-normals-offline.obj";
            std::filesystem::rename(modelPath, offlinePath);
            auto corrected = healthy;
            corrected.position.x = 0.1f;
            corrected.rotation.z = 15.0f;
            corrected.scale.x = -0.2f;
            Check(restored.UpdateTransform(oversizedId, corrected) && restored.IsDirty(),
                  "Valid deferred bounds correction was rejected");
            const auto& correctedView = restored.GetModelViews().front();
            Check(correctedView.loaded && correctedView.error.empty(),
                  "Corrective transform did not attach the cached mesh and clear its error");
            draws = restored.BuildDrawList(glm::mat4(1.0f));
            const auto correctedMatrix = MakeModelMatrix(corrected);
            Check(draws.size() == 2 &&
                      std::all_of(draws.begin(), draws.end(), [cachedArray](const auto& draw) {
                          return draw.vertexArray->GetId() == cachedArray;
                      }) &&
                      std::any_of(draws.begin(), draws.end(), [&](const auto& draw) {
                          return draw.modelViewProjection == correctedMatrix && draw.mirrored &&
                                 draw.normalMatrix == MakeNormalMatrix(correctedMatrix);
                      }),
                  "Correction reimported the mesh or retained stale cached transforms");
            Check(restored.SaveScene() && !restored.IsDirty(),
                  "Corrected instance could not be saved normally");
            const auto saved = config.LoadScene();
            Check(saved.models.size() == 2 && saved.models[0].position == corrected.position &&
                      saved.models[0].rotation == corrected.rotation &&
                      saved.models[0].scale == corrected.scale &&
                      saved.models[1].scale == healthy.scale,
                  "Saving the correction changed or lost an instance");
            std::filesystem::rename(offlinePath, modelPath);
        }
        const auto beforeShutdown = ReadText(restorePath);
        restored.Shutdown();
        Check(!restored.IsDirty() && ReadText(restorePath) == beforeShutdown &&
                  (correct || beforeShutdown == originalBytes),
              "Clean shutdown rewrote the deferred-bounds scene");
        Check(glIsVertexArray(cachedArray) == GL_FALSE && glIsTexture(cachedTexture) == GL_FALSE,
              "Deferred-bounds fixture leaked cached GPU resources");
    }

    FileParser pending(context, root / "pending-bounds.json", settings);
    pending.LoadSavedFiles();
    Check(pending.LoadAsset(modelPath) && pending.LoadAsset(modelPath),
          "Could not queue the pre-upload bounds fixture");
    const ModelId pendingId = pending.GetModelViews().front().id;
    Check(!pending.GetModelViews().front().loaded && pending.UpdateTransform(pendingId, oversized),
          "Extreme transform was not accepted against the pre-upload placeholder bounds");
    waitForBoundsFailure(pending);
    Check(pending.IsDirty() && pending.GetDescription(pendingId)->scale == oversized.scale,
          "Deferred validation lost or clamped an authored pre-upload edit");
    auto collapsed = healthy;
    collapsed.scale.z = 0.0f;
    Check(pending.UpdateTransform(pendingId, collapsed) && pending.GetModelViews().front().loaded &&
              pending.GetModelViews().front().error.empty(),
          "Legal zero scale could not correct an unattached instance");
    const auto collapsedDraws = pending.BuildDrawList(glm::mat4(1.0f));
    Check(collapsedDraws.size() == 2 &&
              std::any_of(collapsedDraws.begin(), collapsedDraws.end(), [&](const auto& draw) {
                  return draw.modelViewProjection == MakeModelMatrix(collapsed) &&
                         draw.normalMatrix == glm::mat3(0.0f) && !draw.mirrored;
              }),
          "Pre-upload correction retained invalid normals or bounds");
    Check(pending.SaveScene() && !pending.IsDirty(), "Could not save the pre-upload correction");
    pending.Shutdown();
    CheckGl();
}

void TestRestoreFailure(const GraphicsContext& context, const std::filesystem::path& root) {
    const auto scenePath = root / "restore-failure.json";
    std::array<ModelDescription, 2> original;
    original[0].filePath = PathToUtf8(root / std::filesystem::path(u8"caf\u00e9-\u6a21\u578b.OBJ"));
    original[1].filePath = PathToUtf8(root / "unsupported.fbx");
    const ConfigManager config(scenePath);
    SceneDocument document;
    document.models.assign(original.begin(), original.end());
    document.lighting.enabled = false;
    Check(config.SaveScene(document), "Could not prepare transactional restore test");
    const auto originalBytes = ReadText(scenePath);
    {
        FileParser assets(context, scenePath);
        bool rejected = false;
        try {
            assets.LoadSavedFiles();
        } catch(const std::runtime_error&) {
            rejected = true;
        }
        Check(rejected, "An incompletely registered scene was accepted");
        auto lighting = assets.GetLighting();
        lighting.ambientStrength = 0.35f;
        Check(assets.UpdateLighting(lighting) && assets.IsDirty(),
              "Could not edit lighting after the failed restore");
        Check(!assets.SaveScene() && assets.IsDirty(),
              "Explicit save bypassed the incomplete-restore guard");
        assets.Shutdown();
        Check(!assets.SaveScene(), "Stopped scene allowed an explicit save");
    }
    const auto saved = config.LoadScene();
    Check(saved.models.size() == original.size() &&
              saved.models[1].filePath == original[1].filePath &&
              saved.lighting == document.lighting && ReadText(scenePath) == originalBytes,
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
        // Two instances of one asset share a single import, so they may become renderable in the
        // same frame; the per-frame policy bounds imports, not shared instances.
        Check(count <= previouslyLoaded + 2, "Per-frame GPU upload budget was exceeded");
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
    SceneLighting unlit;
    unlit.enabled = false;
    renderer.BeginScene(64, 64);
    renderer.DrawScene(draws, unlit);
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

    // Consecutive submissions must not leak front-face winding: a mirrored submission followed by
    // a normal one must still restore GL_CCW for the normal geometry.
    auto winding = assets.BuildDrawList(glm::mat4(1.0f));
    Check(!winding.empty(), "Winding regression test has no geometry");
    for(auto& draw : winding) {
        draw.blended = false;
        draw.alphaCutoff = 0.0f;
    }
    renderer.BeginScene(64, 64);
    for(auto& draw : winding) {
        draw.mirrored = true;
    }
    renderer.DrawScene(winding, unlit);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for(auto& draw : winding) {
        draw.mirrored = false;
    }
    renderer.DrawScene(winding, unlit);
    std::array<unsigned char, 4> windingCenter{};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, windingCenter.data());
    Check(windingCenter[0] > 200 && windingCenter[1] > 200 && windingCenter[2] > 200,
          "Front-face winding leaked between DrawScene submissions");

    // Alpha is an explicit material policy, not a side effect of the shader writing an alpha value.
    renderer.BeginScene(64, 64);
    for(auto& draw : winding) {
        draw.alphaCutoff = 0.9f;
        draw.baseColor = {1.0f, 1.0f, 1.0f, 0.5f};
    }
    renderer.DrawScene(winding, unlit);
    std::array<unsigned char, 4> masked{};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, masked.data());
    Check(masked[0] < 200 && masked[1] < 200 && masked[2] < 200,
          "Alpha-masked fragments were not discarded");
    for(auto& draw : winding) {
        draw.alphaCutoff = 0.0f;
    }
    renderer.DrawScene(winding, unlit);
    std::array<unsigned char, 4> opaque{};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, opaque.data());
    Check(opaque[0] > 200, "Opaque rendering stopped after the masked pass");
    CheckGl();

    // Exercise the actual GLFW drop event path and the editor frame orchestration.
    const auto drop = glfwSetDropCallback(window.GetWindow(), nullptr);
    Check(drop != nullptr, "Window has no file drop event callback");
    const std::string droppedPath = PathToUtf8(modelPath);
    const char* paths[] = {droppedPath.c_str()};
    drop(window.GetWindow(), 1, paths);
    glfwSetDropCallback(window.GetWindow(), drop);
    Editor editor(window, renderer, gui, assets);
    auto authoredLighting = assets.GetLighting();
    authoredLighting.direction = {0.0f, -3.0f, -4.0f};
    authoredLighting.color = {0.5f, 0.75f, 1.0f};
    authoredLighting.intensity = 0.6f;
    authoredLighting.ambientStrength = 0.15f;
    Check(assets.UpdateLighting(authoredLighting), "Populated scene lighting edit failed");
    editor.Run(4);
    Check(assets.GetModelViews().size() == 3,
          "Drag-and-drop did not reach the async asset pipeline");
    Check(assets.UpdateLighting(unlit), "Populated scene could not switch to unlit rendering");
    editor.Run(1);
    Check(assets.UpdateLighting(SceneLighting{}), "Populated scene lighting reset failed");
    edited.rotation = {15.0f, 30.0f, 5.0f};
    edited.scale = {-1.0f, 1.5f, 0.5f};
    Check(assets.UpdateTransform(secondId, edited), "Editor-frame nonuniform transform edit failed");
    editor.Run(1);
    Check(assets.UpdateLighting(authoredLighting), "Populated scene could not re-enable sunlight");
    const auto savedLighting = assets.GetLighting();
    editor.Run(1);
    Check(assets.RemoveModel(firstId) && !assets.GetDescription(firstId),
          "Stable-ID model removal failed");
    Check(assets.GetDescription(secondId).has_value(),
          "Removing a model invalidated another model ID");
    Check(initial[0].description.filePath == PathToUtf8(modelPath),
          "An owned model-name snapshot was invalidated");
    Check(assets.SaveScene() && !assets.IsDirty(), "Scene save reported failure or retained dirty");
    assets.Shutdown();
    assets.Shutdown();
    Check(glIsTexture(textureId) == GL_FALSE, "Scene texture pages outlived shutdown");
    const auto saved = ConfigManager(scenePath).LoadScene();
    Check(saved.models.size() == 2 && saved.models[0].scale.x == -1 &&
              saved.lighting == savedLighting,
          "Shutdown lost pending models or edited transforms");
    const auto savedBytes = ReadText(scenePath);
    {
        FileParser reopened(window.GetContext(), scenePath, importSettings);
        reopened.LoadSavedFiles();
        WaitForModels(reopened, 2);
        Check(reopened.GetLighting() == savedLighting && !reopened.IsDirty(),
              "Populated scene did not restore lighting without authoring an edit");
        Editor reopenedEditor(window, renderer, gui, reopened);
        reopenedEditor.Run(2);
        Check(!reopened.IsDirty(), "Reopened populated editor dirtied the scene without an edit");
    }
    Check(ReadText(scenePath) == savedBytes, "Reopened populated editor rewrote the scene");
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
        TestShaderFailures(window.GetContext(), temporary.path, argv[1]);
        TestTexturePages(window.GetContext(), temporary.path);
        TestLighting(window.GetContext(), argv[1]);
        TestLightingEdits(window, temporary.path, argv[1]);
        TestLightingRestore(window.GetContext(), temporary.path);
        TestLightingSaveFailure(window.GetContext(), temporary.path);
        TestCachedNormalTransforms(window.GetContext(), temporary.path);
        TestScene(window, temporary.path, argv[1]);
        TestRestoreFailure(window.GetContext(), temporary.path);
        std::cout
            << "OpenGL ownership, shaders, uploads, lighting, scene edits, drop, and shutdown tests passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
