#pragma once

#include <glad/glad.h>
#include "VertexArray.h"
#include "IndexBuffer.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

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