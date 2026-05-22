#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "IndexBuffer.h"
#include "VertexArray.h"
#include "TextureArray.h"
#include "IRenderer.h"

class Renderer : public IRenderer {
public:
    static Renderer& GetInstance() {
        static Renderer instance;
        return instance;
    }

    void Draw(VertexArray& va, IndexBuffer& ib, TextureArray& ta);
    void DrawPatches(VertexArray& va, IndexBuffer& ib, TextureArray& ta);
    void UpdateWindowSize(int width, int height);
    void Clear();
    const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }

private:
    Renderer();
    ~Renderer();
    glm::mat4 m_ProjectionMatrix;
};