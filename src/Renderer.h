#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "IndexBuffer.h"
#include "VertexArray.h"
#include "TextureArray.h"

class Renderer {
public:
    Renderer(glm::mat4& projectionMatrixArg);
    ~Renderer();
    void Draw(VertexArray& va, IndexBuffer& ib, TextureArray& ta);
    void DrawPatches(VertexArray& va, IndexBuffer& ib, TextureArray& ta);
    void UpdateWindowSize(int width, int height);
    void Clear();

private:
    glm::mat4& m_ProjectionMatrix;
};