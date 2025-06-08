#pragma once

#include <iostream>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "VertexArray.h"
#include "IndexBuffer.h"


class Renderer {
public:
    // Constructor
    Renderer(glm::mat4& projectionMatrixArg);

    // Destructor
    ~Renderer();

    void Draw(VertexArray& va, IndexBuffer& ib);

    void UpdateWindowSize(int width, int height);

    // Function to clear the screen
    void Clear();
private:
    glm::mat4& m_ProjectionMatrix; // Projection matrix for the renderer
};