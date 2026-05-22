#pragma once

#include "VertexArray.h"
#include "IndexBuffer.h"
#include "TextureArray.h"
#include <glm/glm.hpp>

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void Draw(VertexArray& va, IndexBuffer& ib, TextureArray& ta) = 0;
    virtual void Clear() = 0;
    virtual void UpdateWindowSize(int width, int height) = 0;
    virtual const glm::mat4& GetProjectionMatrix() const = 0;
};