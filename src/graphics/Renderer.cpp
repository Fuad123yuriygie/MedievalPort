#include "graphics/Renderer.h"

#include "graphics/GraphicsContext.h"
#include "graphics/Shader.h"
#include "graphics/SkyboxSystem.h"
#include "graphics/TextureArray.h"
#include "graphics/VertexArray.h"

#include <glad/glad.h>
#include <optional>

Renderer::Renderer(const GraphicsContext& context, const std::filesystem::path& assetRoot,
                   const RenderSettings& settings)
    : context(context), settings(settings),
      shader(std::make_unique<Shader>(context, assetRoot / "shaders" / "Basic")),
      skybox(std::make_unique<SkyboxSystem>(context, assetRoot)) {
    // Validate the interface this renderer depends on instead of silently ignoring -1.
    mvpLocation = shader->RequireUniformLocation("u_MVP", GL_FLOAT_MAT4);
    layerLocation = shader->RequireUniformLocation("u_TextureLayer", GL_INT);
    baseColorLocation = shader->RequireUniformLocation("u_BaseColor", GL_FLOAT_VEC4);
    uvTransformLocation = shader->RequireUniformLocation("u_UvTransform", GL_FLOAT_VEC4);
    uvWrapLocation = shader->RequireUniformLocation("u_UvWrap", GL_INT);
    alphaCutoffLocation = shader->RequireUniformLocation("u_AlphaCutoff", GL_FLOAT);
    shader->SetUniform1i(shader->RequireUniformLocation("u_TextureArray", GL_SAMPLER_2D_ARRAY), 0);
    outputSrgb = DetectSrgbFramebuffer();
}

Renderer::~Renderer() {
    Shutdown();
}

void Renderer::Shutdown() noexcept {
    context.AssertCurrent();
    skybox.reset();
    shader.reset();
}

bool Renderer::DetectSrgbFramebuffer() const {
    context.RequireCurrent();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GLint encoding = GL_LINEAR;
    glGetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);
    const bool valid = glGetError() == GL_NO_ERROR;
    return valid && encoding == GL_SRGB;
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
    // Encode linear shader output exactly once when the default framebuffer is sRGB capable.
    if(outputSrgb) {
        glEnable(GL_FRAMEBUFFER_SRGB);
    }
    else {
        glDisable(GL_FRAMEBUFFER_SRGB);
    }
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
    // Winding is established from the first draw of this submission, never assumed from a stale
    // local default. A previous submission may have left GL_CW active.
    std::optional<bool> frontFaceCcw;
    std::optional<bool> blendEnabled;
    const void* lastObject = nullptr;
    bool haveObject = false;
    std::uint32_t lastLayer = 0;
    bool haveLayer = false;
    glm::vec4 lastColor(0.0f);
    bool haveColor = false;
    glm::vec4 lastUv(0.0f);
    bool haveUv = false;
    int lastWrap = 0;
    bool haveWrap = false;
    float lastCutoff = 0.0f;
    bool haveCutoff = false;
    for(const auto& draw : draws) {
        const bool ccw = !draw.mirrored;
        if(!frontFaceCcw || *frontFaceCcw != ccw) {
            frontFaceCcw = ccw;
            glFrontFace(ccw ? GL_CCW : GL_CW);
        }
        // Opaque and masked materials keep depth writes and no blending; only blended materials
        // change the pass state. The list is pre-sorted with blended draws last and back to front.
        if(!blendEnabled || *blendEnabled != draw.blended) {
            blendEnabled = draw.blended;
            if(draw.blended) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
            }
            else {
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
            }
        }
        if(draw.textureArray != boundTexture) {
            draw.textureArray->Bind(0);
            boundTexture = draw.textureArray;
        }
        if(draw.vertexArray != boundVertexArray) {
            draw.vertexArray->Bind();
            boundVertexArray = draw.vertexArray;
        }
        // Submeshes of one object share its transform, so the MVP is uploaded once per object.
        if(!haveObject || lastObject != draw.objectKey) {
            shader->SetUniformMat4f(mvpLocation, draw.modelViewProjection);
            lastObject = draw.objectKey;
            haveObject = true;
        }
        if(!haveLayer || lastLayer != draw.textureLayer) {
            shader->SetUniform1i(layerLocation, static_cast<int>(draw.textureLayer));
            lastLayer = draw.textureLayer;
            haveLayer = true;
        }
        if(!haveColor || lastColor != draw.baseColor) {
            shader->SetUniform4f(baseColorLocation, draw.baseColor);
            lastColor = draw.baseColor;
            haveColor = true;
        }
        if(!haveUv || lastUv != draw.uvTransform) {
            shader->SetUniform4f(uvTransformLocation, draw.uvTransform);
            lastUv = draw.uvTransform;
            haveUv = true;
        }
        if(!haveWrap || lastWrap != draw.uvWrap) {
            shader->SetUniform1i(uvWrapLocation, draw.uvWrap);
            lastWrap = draw.uvWrap;
            haveWrap = true;
        }
        if(!haveCutoff || lastCutoff != draw.alphaCutoff) {
            shader->SetUniform1f(alphaCutoffLocation, draw.alphaCutoff);
            lastCutoff = draw.alphaCutoff;
            haveCutoff = true;
        }
        const auto offset = static_cast<std::uintptr_t>(draw.firstIndex) * sizeof(std::uint32_t);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(draw.indexCount),
                       GL_UNSIGNED_INT,
                       reinterpret_cast<const void*>(offset));
    }
    // Leave the pass in its opaque default so later passes see a predictable depth-write state.
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
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
    // UI is authored in its own encoding; do not apply the scene's linear-to-sRGB conversion.
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}
