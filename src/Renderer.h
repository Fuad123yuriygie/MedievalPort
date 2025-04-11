#pragma once

#include <glad/glad.h>
#include "VertexArray.h"
#include "IndexBuffer.h"

class Renderer {
public:
    // Constructor
    Renderer();

    // Destructor
    ~Renderer();

    void Draw(VertexArray& va, IndexBuffer& ib);

    // Function to clear the screen
    void Clear();
};