#include "Renderer.h"

Renderer::Renderer() {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK); // Cull back faces
    glFrontFace(GL_CCW); // Counter-clockwise winding is front-facing
}

Renderer::~Renderer() {
    // Destructor to clean up resources if needed
    // Currently, no specific cleanup is required for the Renderer class
}

void Renderer::Draw(VertexArray& va, IndexBuffer& ib) {
    va.Bind();
    ib.Bind();
    glDrawElements(GL_TRIANGLES, ib.GetCount(), GL_UNSIGNED_INT, 0);
}

// Function to clear the screen
void Renderer::Clear() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}