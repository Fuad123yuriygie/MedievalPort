#pragma once

#include "core/Settings.h"
#include "graphics/GlObject.h"
#include "graphics/Shader.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"

#include <filesystem>
#include <glm/mat4x4.hpp>

class SkyboxSystem {
public:
    SkyboxSystem(const GraphicsContext& context, const std::filesystem::path& assetRoot,
                 int textureSize = ImportSettings::defaultTextureSize);
    ~SkyboxSystem() = default;
    SkyboxSystem(const SkyboxSystem&) = delete;
    SkyboxSystem& operator=(const SkyboxSystem&) = delete;

    void Render(const glm::mat4& view, const glm::mat4& projection) const;

private:
    Shader shader;
    VertexBuffer vertexBuffer;
    VertexArray vertexArray;
    GlObject cubemap;
    int viewLocation = -1;
    int projectionLocation = -1;
};
