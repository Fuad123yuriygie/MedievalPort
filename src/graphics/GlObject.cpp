#include "graphics/GlObject.h"

#include "graphics/GraphicsContext.h"

#include <glad/glad.h>
#include <stdexcept>
#include <utility>

GlObject::GlObject(const GraphicsContext& context, Kind kind, unsigned textureTarget)
    : context(&context), kind(kind) {
    context.RequireCurrent();
    switch(kind) {
    case Kind::Buffer:
        glCreateBuffers(1, &id);
        break;
    case Kind::VertexArray:
        glCreateVertexArrays(1, &id);
        break;
    case Kind::Texture:
        if(textureTarget != GL_TEXTURE_2D_ARRAY && textureTarget != GL_TEXTURE_CUBE_MAP) {
            throw std::invalid_argument("Unsupported texture target");
        }
        glCreateTextures(textureTarget, 1, &id);
        break;
    case Kind::Program:
        id = glCreateProgram();
        break;
    }
    if(id == 0) {
        throw std::runtime_error("OpenGL object creation failed");
    }
}

GlObject::~GlObject() {
    Reset();
}

GlObject::GlObject(GlObject&& other) noexcept
    : context(std::exchange(other.context, nullptr)), kind(other.kind),
      id(std::exchange(other.id, 0)) {
}

GlObject& GlObject::operator=(GlObject&& other) noexcept {
    if(this != &other) {
        Reset();
        context = std::exchange(other.context, nullptr);
        kind = other.kind;
        id = std::exchange(other.id, 0);
    }
    return *this;
}

void GlObject::RequireCurrent() const {
    if(!context || id == 0) {
        throw std::logic_error("Cannot use a moved-from OpenGL object");
    }
    context->RequireCurrent();
}

void GlObject::Reset() noexcept {
    if(id == 0) {
        return;
    }
    context->AssertCurrent();
    switch(kind) {
    case Kind::Buffer:
        glDeleteBuffers(1, &id);
        break;
    case Kind::VertexArray:
        glDeleteVertexArrays(1, &id);
        break;
    case Kind::Texture:
        glDeleteTextures(1, &id);
        break;
    case Kind::Program:
        glDeleteProgram(id);
        break;
    }
    id = 0;
}
