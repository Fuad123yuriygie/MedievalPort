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
    static WindowSystem& GetInstance() {
        static WindowSystem instance;
        return instance;
    }

    GLFWwindow* GetWindow();

    // void Update();
    // void SetWindowSize(int width, int height);
    // void SetWindowTitle(const char* title);
    // void SetWindowPosition(int x, int y);
    // void SetFullscreen(bool fullscreen);
    // void SetVSync(bool vsync);
    // void SetResizable(bool resizable);
    // void SetIcon(const char* iconPath);
private:
    WindowSystem();
    ~WindowSystem();
    GLFWwindow* window;
    static void DropCallback(GLFWwindow* window, int count, const char** paths);
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
};