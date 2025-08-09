#include "WindowSystem.h"

WindowSystem::WindowSystem(FileParser& fileParserArg) : fileParser(fileParserArg) {
    // Initialize the window system
    // This could include setting up GLFW, creating a window, etc.
    // Init library
    if(!glfwInit()) {
        exit(EXIT_FAILURE);
    }

    // Request an OpenGL debug context
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    // Make window resizable
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // Create windowed mode window and its OpenGL context
    window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "OpenGL", NULL, NULL);
    if(!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);
    // glfwSwapInterval(1); // Enable vsync

    // Set drop callback and pass 'this' as user pointer
    glfwSetWindowUserPointer(window, this);
    glfwSetDropCallback(window, DropCallback);

    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
}

WindowSystem::~WindowSystem() {
    // Cleanup code
    glfwDestroyWindow(window);
    glfwTerminate();
}

GLFWwindow* WindowSystem::GetWindow() {
    return window;
}

void WindowSystem::DropCallback(GLFWwindow* window, int count, const char** paths) {
    WindowSystem* self = static_cast<WindowSystem*>(glfwGetWindowUserPointer(window));
    if(!self)
        return;

    for(int i = 0; i < count; i++) {
        std::string filePath = paths[i];
        std::cout << "Dropped file: " << filePath << std::endl;

        self->fileParser.ExtractFileData(filePath);
    }
}

void WindowSystem::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    WindowSystem* self = static_cast<WindowSystem*>(glfwGetWindowUserPointer(window));
    if(!self)
        return;

    glViewport(0, 0, width, height);
    if(self->renderer) {
        self->renderer->UpdateWindowSize(width, height);
    }
}