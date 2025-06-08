#pragma once

#include <glad/glad.h>

#include "VertexBufferLayout.h"

class VertexBuffer {
public:
    VertexBuffer(const void* data, unsigned int size);
    ~VertexBuffer();
    VertexBufferLayout& GetLayout();
    void Bind() const;
    void Unbind() const;
private:
    unsigned int m_RendererID;
    VertexBufferLayout layout;
};
