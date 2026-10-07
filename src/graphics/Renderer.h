#pragma once

#include "core/Settings.h"

#include <cstdint>
#include <filesystem>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <span>

class GraphicsContext;
class Shader;
class SkyboxSystem;
class TextureArray;
class VertexArray;

// This renderer is an unlit textured renderer: each draw multiplies a diffuse texture slice by a
// per-material base color with optional UV transform, wrapping clamp and alpha masking/blending.
// No lighting model is implemented, so authored lighting properties are reported as unsupported
// during import rather than silently discarded.

// Frame-scoped, immutable views. The scene cannot be edited during submission.
struct DrawItem {
    const VertexArray* vertexArray = nullptr;
    const TextureArray* textureArray = nullptr;
    // Stable per-instance identity so submeshes of one object do not re-upload its transform.
    const void* objectKey = nullptr;
    glm::mat4 modelViewProjection{1.0f};
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
    void DrawScene(std::span<const DrawItem> draws) const;
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
    int layerLocation = -1;
    int baseColorLocation = -1;
    int uvTransformLocation = -1;
    int uvWrapLocation = -1;
    int alphaCutoffLocation = -1;
    bool outputSrgb = false;
};
