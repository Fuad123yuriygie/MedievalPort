#include "graphics/VertexBuffer.h"

#include <glad/glad.h>
#include <limits>
#include <stdexcept>

VertexBuffer::VertexBuffer(const GraphicsContext& context, std::span<const std::byte> data)
    : object(context, GlObject::Kind::Buffer) {
    if(data.empty() ||
       data.size() > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max())) {
        throw std::invalid_argument("Vertex buffer data is empty or too large");
    }
    const auto size = static_cast<GLsizeiptr>(data.size());
    glNamedBufferStorage(object.GetId(), size, data.data(), 0);
    GLint64 allocated = 0;
    glGetNamedBufferParameteri64v(object.GetId(), GL_BUFFER_SIZE, &allocated);
    if(allocated != size) {
        throw std::runtime_error("Vertex buffer allocation failed");
    }
}
