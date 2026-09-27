#pragma once

#include "graphics/GlObject.h"

#include <cstddef>
#include <span>

class VertexBuffer {
public:
    VertexBuffer(const GraphicsContext& context, std::span<const std::byte> data);
    ~VertexBuffer() = default;
    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;
    VertexBuffer(VertexBuffer&&) noexcept = default;
    VertexBuffer& operator=(VertexBuffer&&) noexcept = default;

    unsigned GetId() const {
        return object.GetId();
    }

private:
    GlObject object;
};
