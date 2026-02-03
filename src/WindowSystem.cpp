#include "WindowSystem.h"
#include "ModelLoaderThread.h"

// Make window system a singleton
WindowSystem::WindowSystem() {
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

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << glGetString(GL_VERSION) << std::endl;

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
        std::cout << "Dropped file, queuing for async load: " << filePath << std::endl;

        // Queue the file for asynchronous loading with a callback
        // The callback will be invoked when loading completes
        ModelLoaderThread::GetInstance().QueueModelLoad(filePath, 
            [](const PendingModelData& pendingData) {
                // This callback happens on the worker thread
                // Post the model to FileParser to add to the main list
                FileParser::GetInstance().AddModelFromLoader(pendingData);
            });
    }
}

void WindowSystem::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    WindowSystem* self = static_cast<WindowSystem*>(glfwGetWindowUserPointer(window));
    if(!self)
        return;

    glViewport(0, 0, width, height);
    Renderer::GetInstance().UpdateWindowSize(width, height);
}

