#pragma once

#include "core/Settings.h"

#include <cstdint>
#include <filesystem>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <span>

class GraphicsContext;
struct SceneLighting;
class Shader;
class SkyboxSystem;
class TextureArray;
class VertexArray;

// Each draw multiplies a diffuse texture slice by a per-material base color with optional UV
// transform, wrapping clamp and alpha masking/blending. Optional directional Lambert sunlight
// and scalar ambient fill affect RGB only; imported OBJ ambient/specular material properties
// remain unsupported and are reported during import.

// Frame-scoped, immutable views. The scene cannot be edited during submission.
struct DrawItem {
    const VertexArray* vertexArray = nullptr;
    const TextureArray* textureArray = nullptr;
    // Stable per-instance identity so submeshes of one object do not re-upload its transform.
    const void* objectKey = nullptr;
    glm::mat4 modelViewProjection{1.0f};
    glm::mat3 normalMatrix{1.0f};
    glm::vec4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 uvTransform{1.0f, 1.0f, 0.0f, 0.0f}; // xy scale, zw offset
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t textureLayer = 0;
    int uvWrap = 0; // 0 repeat, 1 clamp
    float alphaCutoff = 0.0f;
    float distanceSquared = 0.0f;
    bool mirrored = false;
    bool blended = false;
};

class Renderer {
public:
    Renderer(const GraphicsContext& context, const std::filesystem::path& assetRoot,
             const RenderSettings& settings = {});
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void BeginScene(int width, int height) const;
    void DrawScene(std::span<const DrawItem> draws, const SceneLighting& lighting) const;
    void DrawSkybox(const glm::mat4& view, const glm::mat4& projection) const;
    void BeginUI() const;
    void Shutdown() noexcept;

private:
    bool DetectSrgbFramebuffer() const;

    const GraphicsContext& context;
    RenderSettings settings;
    std::unique_ptr<Shader> shader;
    std::unique_ptr<SkyboxSystem> skybox;
    int mvpLocation = -1;
    int normalMatrixLocation = -1;
    int layerLocation = -1;
    int baseColorLocation = -1;
    int uvTransformLocation = -1;
    int uvWrapLocation = -1;
    int alphaCutoffLocation = -1;
    int lightingEnabledLocation = -1;
    int sunDirectionLocation = -1;
    int sunColorLocation = -1;
    int sunIntensityLocation = -1;
    int ambientStrengthLocation = -1;
    bool outputSrgb = false;
};
