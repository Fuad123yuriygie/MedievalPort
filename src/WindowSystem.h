#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

#include "FileParser.h"
#include "IndexBuffer.h"
#include "Renderer.h"
#include "VertexArray.h"
#include "VertexBuffer.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

class WindowSystem {
public:
    WindowSystem(FileParser& fileParserArg);
    ~WindowSystem();
    GLFWwindow* GetWindow();
    void SetRenderer(Renderer& rendererArg) {
        renderer = &rendererArg;
    }

private:
    GLFWwindow* window;
    FileParser& fileParser;
    Renderer* renderer = nullptr;
    static void DropCallback(GLFWwindow* window, int count, const char** paths);
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
};