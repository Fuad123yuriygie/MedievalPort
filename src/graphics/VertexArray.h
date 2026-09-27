#pragma once

#include "graphics/GlObject.h"

class IndexBuffer;
class VertexBuffer;
class VertexBufferLayout;

class VertexArray {
public:
    explicit VertexArray(const GraphicsContext& context);
    ~VertexArray() = default;
    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;
    VertexArray(VertexArray&&) noexcept = default;
    VertexArray& operator=(VertexArray&&) noexcept = default;

    void AddBuffer(const VertexBuffer& buffer, const VertexBufferLayout& layout,
                   unsigned binding = 0);
    void SetIndexBuffer(const IndexBuffer& buffer);
    void Bind() const;
    unsigned GetId() const {
        return object.GetId();
    }

private:
    GlObject object;
};
