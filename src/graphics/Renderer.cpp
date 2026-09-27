#include "graphics/Renderer.h"

#include "graphics/GraphicsContext.h"
#include "graphics/Shader.h"
#include "graphics/SkyboxSystem.h"
#include "graphics/TextureArray.h"
#include "graphics/VertexArray.h"

#include <glad/glad.h>

Renderer::Renderer(const GraphicsContext& context, const std::filesystem::path& assetRoot,
                   const RenderSettings& settings)
    : context(context), settings(settings),
      shader(std::make_unique<Shader>(context, assetRoot / "shaders" / "Basic")),
      skybox(std::make_unique<SkyboxSystem>(context, assetRoot)) {
    mvpLocation = shader->GetUniformLocation("u_MVP");
    layerLocation = shader->GetUniformLocation("u_TextureLayer");
    shader->SetUniform1i(shader->GetUniformLocation("u_TextureArray"), 0);
}

Renderer::~Renderer() {
    Shutdown();
}

void Renderer::Shutdown() noexcept {
    context.AssertCurrent();
    skybox.reset();
    shader.reset();
}

void Renderer::BeginScene(int width, int height) const {
    context.RequireCurrent();
    if(width <= 0 || height <= 0) {
        return;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    if(settings.samples > 0) {
        glEnable(GL_MULTISAMPLE);
    }
    else {
        glDisable(GL_MULTISAMPLE);
    }
    glClearColor(settings.clearColor.r,
                 settings.clearColor.g,
                 settings.clearColor.b,
                 settings.clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::DrawScene(std::span<const DrawItem> draws) const {
    context.RequireCurrent();
    shader->Bind();
    const TextureArray* boundTexture = nullptr;
    const VertexArray* boundVertexArray = nullptr;
    bool mirrored = false;
    for(const auto& draw : draws) {
        if(draw.textureArray != boundTexture) {
            draw.textureArray->Bind(0);
            boundTexture = draw.textureArray;
        }
        if(draw.vertexArray != boundVertexArray) {
            draw.vertexArray->Bind();
            boundVertexArray = draw.vertexArray;
        }
        if(draw.mirrored != mirrored) {
            mirrored = draw.mirrored;
            glFrontFace(mirrored ? GL_CW : GL_CCW);
        }
        shader->SetUniformMat4f(mvpLocation, draw.modelViewProjection);
        shader->SetUniform1i(layerLocation, static_cast<int>(draw.textureLayer));
        const auto offset = static_cast<std::uintptr_t>(draw.firstIndex) * sizeof(std::uint32_t);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(draw.indexCount),
                       GL_UNSIGNED_INT,
                       reinterpret_cast<const void*>(offset));
    }
}

void Renderer::DrawSkybox(const glm::mat4& view, const glm::mat4& projection) const {
    context.RequireCurrent();
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    skybox->Render(view, projection);
}

void Renderer::BeginUI() const {
    context.RequireCurrent();
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}
