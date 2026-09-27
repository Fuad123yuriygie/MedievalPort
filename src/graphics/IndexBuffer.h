#pragma once

#include "graphics/GlObject.h"

#include <cstdint>
#include <span>

class IndexBuffer {
public:
    IndexBuffer(const GraphicsContext& context, std::span<const std::uint32_t> indices);
    ~IndexBuffer() = default;
    IndexBuffer(const IndexBuffer&) = delete;
    IndexBuffer& operator=(const IndexBuffer&) = delete;
    IndexBuffer(IndexBuffer&&) noexcept = default;
    IndexBuffer& operator=(IndexBuffer&&) noexcept = default;

    unsigned GetId() const {
        return object.GetId();
    }
    std::uint32_t GetCount() const {
        return count;
    }

private:
    GlObject object;
    std::uint32_t count = 0;
};
