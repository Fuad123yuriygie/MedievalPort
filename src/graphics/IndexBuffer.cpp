#include "graphics/IndexBuffer.h"

#include <glad/glad.h>
#include <limits>
#include <stdexcept>

IndexBuffer::IndexBuffer(const GraphicsContext& context, std::span<const std::uint32_t> indices)
    : object(context, GlObject::Kind::Buffer) {
    if(indices.empty() ||
       indices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) ||
       indices.size_bytes() > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max())) {
        throw std::invalid_argument("Index buffer data is empty or too large");
    }
    count = static_cast<std::uint32_t>(indices.size());
    const auto size = static_cast<GLsizeiptr>(indices.size_bytes());
    glNamedBufferStorage(object.GetId(), size, indices.data(), 0);
    GLint64 allocated = 0;
    glGetNamedBufferParameteri64v(object.GetId(), GL_BUFFER_SIZE, &allocated);
    if(allocated != size) {
        throw std::runtime_error("Index buffer allocation failed");
    }
}
