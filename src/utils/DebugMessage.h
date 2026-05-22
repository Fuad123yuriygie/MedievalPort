#pragma once

#include <glad/glad.h>
#include <iostream>

class DebugMessage {
public:
    DebugMessage();
    ~DebugMessage();

private:
    static void APIENTRY openglDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                             GLsizei length, const GLchar* message,
                                             const void* userParam);
    void setupDebugCallback();
};