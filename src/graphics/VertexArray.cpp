#include "graphics/VertexArray.h"

#include "graphics/IndexBuffer.h"
#include "graphics/VertexBuffer.h"
#include "graphics/VertexBufferLayout.h"

#include <glad/glad.h>
#include <stdexcept>

VertexArray::VertexArray(const GraphicsContext& context)
    : object(context, GlObject::Kind::VertexArray) {
}

void VertexArray::AddBuffer(const VertexBuffer& buffer, const VertexBufferLayout& layout,
                            unsigned binding) {
    object.RequireCurrent();
    GLint maxAttributes = 0;
    GLint maxBindings = 0;
    GLint maxStride = 0;
    GLint maxOffset = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttributes);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &maxBindings);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIB_STRIDE, &maxStride);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET, &maxOffset);
    if(buffer.GetId() == 0 || layout.GetStride() == 0 ||
       layout.GetStride() > static_cast<unsigned>(maxStride) ||
       binding >= static_cast<unsigned>(maxBindings)) {
        throw std::invalid_argument("Invalid vertex buffer binding or stride");
    }
    for(const auto& element : layout.GetElements()) {
        if(element.location >= static_cast<unsigned>(maxAttributes) ||
           element.offset > static_cast<unsigned>(maxOffset)) {
            throw std::invalid_argument("Vertex attribute exceeds the driver limits");
        }
    }
    glVertexArrayVertexBuffer(object.GetId(),
                              binding,
                              buffer.GetId(),
                              0,
                              static_cast<GLsizei>(layout.GetStride()));
    for(const auto& element : layout.GetElements()) {
        glEnableVertexArrayAttrib(object.GetId(), element.location);
        if(element.type == GL_INT || element.type == GL_UNSIGNED_INT) {
            glVertexArrayAttribIFormat(object.GetId(),
                                       element.location,
                                       static_cast<GLint>(element.count),
                                       element.type,
                                       element.offset);
        }
        else {
            glVertexArrayAttribFormat(object.GetId(),
                                      element.location,
                                      static_cast<GLint>(element.count),
                                      element.type,
                                      element.normalized,
                                      element.offset);
        }
        glVertexArrayAttribBinding(object.GetId(), element.location, binding);
    }
}

void VertexArray::SetIndexBuffer(const IndexBuffer& buffer) {
    object.RequireCurrent();
    if(buffer.GetId() == 0) {
        throw std::invalid_argument("Cannot attach a moved-from index buffer");
    }
    glVertexArrayElementBuffer(object.GetId(), buffer.GetId());
}

void VertexArray::Bind() const {
    object.RequireCurrent();
    glBindVertexArray(object.GetId());
}
