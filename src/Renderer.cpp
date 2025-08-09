#include "Renderer.h"

Renderer::Renderer(glm::mat4& projectionMatrixArg) : m_ProjectionMatrix(projectionMatrixArg) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_BACK); // Cull back faces
    glFrontFace(GL_CCW); // Counter-clockwise winding is front-facing
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

Renderer::~Renderer() {
    // Destructor to clean up resources if needed
}

void Renderer::Draw(VertexArray& va, IndexBuffer& ib, TextureArray& ta) {
    va.Bind();
    ib.Bind();
    ta.Bind();
    glDrawElements(GL_TRIANGLES, ib.GetCount(), GL_UNSIGNED_INT, 0);
}

// Clear the screen
void Renderer::Clear() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::UpdateWindowSize(int width, int height) {
    glViewport(0, 0, width, height);
    m_ProjectionMatrix =
        glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);
    std::cout << "Updated projection matrix with new window size: " << width << "x" << height
              << std::endl;
}

void Renderer::DrawPatches(VertexArray& va, IndexBuffer& ib, TextureArray& ta) {
    va.Bind();
    ib.Bind();
    ta.Bind();
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glDrawElements(GL_PATCHES, ib.GetCount(), GL_UNSIGNED_INT, 0);
}
