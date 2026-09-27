#include "graphics/SkyboxSystem.h"

#include "graphics/TextureUpload.h"
#include "graphics/VertexBufferLayout.h"
#include "io/ImageData.h"
#include "utils/Log.h"

#include <array>
#include <glad/glad.h>
#include <glm/mat3x3.hpp>
#include <span>
#include <stdexcept>

namespace
{
constexpr auto MakeCubeVertices() {
    constexpr std::array<std::array<float, 3>, 8> corners{{{-1, -1, -1},
                                                           {1, -1, -1},
                                                           {1, 1, -1},
                                                           {-1, 1, -1},
                                                           {-1, -1, 1},
                                                           {1, -1, 1},
                                                           {1, 1, 1},
                                                           {-1, 1, 1}}};
    constexpr std::array<unsigned, 36> indices{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
                                               0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5,
                                               3, 7, 6, 3, 6, 2, 0, 1, 5, 0, 5, 4};
    std::array<float, indices.size() * 3> vertices{};
    for(std::size_t index = 0; index < indices.size(); ++index) {
        for(std::size_t component = 0; component < 3; ++component) {
            vertices[index * 3 + component] = corners[indices[index]][component];
        }
    }
    return vertices;
}
constexpr auto cubeVertices = MakeCubeVertices();
} // namespace

SkyboxSystem::SkyboxSystem(const GraphicsContext& context, const std::filesystem::path& assetRoot,
                           int textureSize)
    : shader(context, assetRoot / "shaders" / "SkySphere"),
      vertexBuffer(context, std::as_bytes(std::span(cubeVertices))), vertexArray(context),
      cubemap(context, GlObject::Kind::Texture, GL_TEXTURE_CUBE_MAP) {
    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maxSize);
    if(textureSize <= 0 || textureSize > maxSize) {
        throw std::invalid_argument("Cubemap size exceeds the driver limit");
    }
    VertexBufferLayout layout;
    layout.Push<float>(0, 3);
    vertexArray.AddBuffer(vertexBuffer, layout);

    constexpr std::array<const char*, 6> faces{"px.png",
                                               "nx.png",
                                               "py.png",
                                               "ny.png",
                                               "pz.png",
                                               "nz.png"};
    std::array<DecodedImage, faces.size()> images;
    for(std::size_t face = 0; face < faces.size(); ++face) {
        const auto path = assetRoot / "textures" / faces[face];
        try {
            images[face] = DecodeImage(path, false, textureSize, textureSize);
        } catch(const std::exception& error) {
            Log(LogLevel::Warning, "Cubemap fallback for " + path.string() + ": " + error.what());
            images[face] = MakeSolidImage("skybox-fallback", textureSize, {80, 100, 140, 255});
        }
    }
    glTextureStorage2D(cubemap.GetId(), 1, GL_RGBA8, textureSize, textureSize);
    GLint allocatedWidth = 0;
    glGetTextureLevelParameteriv(cubemap.GetId(), 0, GL_TEXTURE_WIDTH, &allocatedWidth);
    if(allocatedWidth != textureSize) {
        throw std::runtime_error("Cubemap allocation failed");
    }
    {
        const PixelUnpackScope unpack;
        for(std::size_t face = 0; face < images.size(); ++face) {
            glTextureSubImage3D(cubemap.GetId(),
                                0,
                                0,
                                0,
                                static_cast<GLint>(face),
                                textureSize,
                                textureSize,
                                1,
                                GL_RGBA,
                                GL_UNSIGNED_BYTE,
                                images[face].pixels.data());
        }
    }
    glTextureParameteri(cubemap.GetId(), GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(cubemap.GetId(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(cubemap.GetId(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(cubemap.GetId(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(cubemap.GetId(), GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    viewLocation = shader.GetUniformLocation("u_View");
    projectionLocation = shader.GetUniformLocation("u_Projection");
    shader.SetUniform1i(shader.GetUniformLocation("skybox"), 0);
}

void SkyboxSystem::Render(const glm::mat4& view, const glm::mat4& projection) const {
    shader.Bind();
    shader.SetUniformMat4f(viewLocation, glm::mat4(glm::mat3(view)));
    shader.SetUniformMat4f(projectionLocation, projection);
    vertexArray.Bind();
    glBindTextureUnit(0, cubemap.GetId());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(cubeVertices.size() / 3));
}
