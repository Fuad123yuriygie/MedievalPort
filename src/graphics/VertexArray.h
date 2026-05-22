#pragma once

#include "VertexBuffer.h"
#include "VertexBufferLayout.h"

class VertexArray {
private:
    unsigned int m_RendererID;

public:
    VertexArray();
    ~VertexArray();
    void AddBuffer(VertexBuffer& vb, std::vector<int>& MaterialIndices);
    void Bind() const;
    void Unbind() const;
};