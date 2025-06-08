#include "DebugMessage.h"

DebugMessage::DebugMessage() {
    // Set up the debug callback
    setupDebugCallback();
}

DebugMessage::~DebugMessage() {
    // Cleanup if needed
}

void APIENTRY DebugMessage::openglDebugCallback(GLenum source, GLenum type, GLuint id,
                                   GLenum severity, GLsizei length,
                                   const GLchar* message, const void* userParam) {
    std::cerr << "OpenGL Debug Message: " << message << std::endl;
}

void DebugMessage::setupDebugCallback() {
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(openglDebugCallback, nullptr);
}