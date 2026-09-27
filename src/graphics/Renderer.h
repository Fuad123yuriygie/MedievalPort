#pragma once

#include "core/Settings.h"

#include <cstdint>
#include <filesystem>
#include <glm/mat4x4.hpp>
#include <memory>
#include <span>

class GraphicsContext;
class Shader;
class SkyboxSystem;
class TextureArray;
class VertexArray;

// Frame-scoped, immutable views. The scene cannot be edited during submission.
struct DrawItem {
    const VertexArray* vertexArray = nullptr;
    const TextureArray* textureArray = nullptr;
    glm::mat4 modelViewProjection{1.0f};
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t textureLayer = 0;
    bool mirrored = false;
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
    const GraphicsContext& context;
    RenderSettings settings;
    std::unique_ptr<Shader> shader;
    std::unique_ptr<SkyboxSystem> skybox;
    int mvpLocation = -1;
    int layerLocation = -1;
};
