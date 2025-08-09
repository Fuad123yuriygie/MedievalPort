#include "VertexArray.h"
#include "Renderer.h"

VertexArray::VertexArray() {
    glGenVertexArrays(1, &m_RendererID);
}

VertexArray::~VertexArray() {
    glDeleteVertexArrays(1, &m_RendererID);
}

void VertexArray::AddBuffer(VertexBuffer& vb, std::vector<int>& MaterialIndices) {
    Bind();
    vb.Bind();
    const auto& elements = vb.GetLayout().GetElements();
    unsigned int offset = 0;
    for(unsigned int i = 0; i < elements.size(); i++) {
        const auto& element = elements[i];
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i,
                              element.count,
                              element.type,
                              element.normalized,
                              vb.GetLayout().GetStride(),
                              (const void*)(uintptr_t)offset);
        offset += element.count * VertexBufferElement::GetSizeOfType(element.type);
    }

    // A sloppy way to add texture vertex array
    GLuint texIDVBO;
    glGenBuffers(1, &texIDVBO);
    glBindBuffer(GL_ARRAY_BUFFER, texIDVBO);
    glBufferData(GL_ARRAY_BUFFER, MaterialIndices.size() * sizeof(int), MaterialIndices.data(), GL_STATIC_DRAW);

    // 2. Enable and set the attribute pointer for location 3
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 1, GL_INT, 0, (void*)0); // Note the 'I' for integer attributes
}

void VertexArray::Bind() const {
    glBindVertexArray(m_RendererID);
}

void VertexArray::Unbind() const {
    glBindVertexArray(0);
}