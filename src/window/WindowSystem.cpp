#include "window/WindowSystem.h"

#include "graphics/GraphicsContext.h"
#include "io/PathUtils.h"
#include "utils/DebugMessage.h"
#include "utils/Log.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <stdexcept>
#include <string>

WindowSystem::WindowSystem(const RenderSettings& settings) {
    if(settings.width <= 0 || settings.height <= 0 || settings.samples < 0) {
        throw std::invalid_argument("Invalid window dimensions or MSAA sample count");
    }
    glfwSetErrorCallback(ErrorCallback);
    if(!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    try {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#ifndef NDEBUG
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE, settings.visible ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_SAMPLES, settings.samples);
        window =
            glfwCreateWindow(settings.width, settings.height, "MedievalPort", nullptr, nullptr);
        if(!window) {
            throw std::runtime_error("An OpenGL 4.5 core context is required");
        }
        glfwMakeContextCurrent(window);
        if(!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            throw std::runtime_error("Failed to initialize GLAD");
        }

        GLint profile = 0;
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
        if(!GLAD_GL_VERSION_4_5 || (profile & GL_CONTEXT_CORE_PROFILE_BIT) == 0) {
            throw std::runtime_error("The driver did not provide OpenGL 4.5 core");
        }
        context = std::make_unique<GraphicsContext>(window);
        InstallDebugOutput(*context);
        Log(LogLevel::Info, reinterpret_cast<const char*>(glGetString(GL_VERSION)));
        glfwSwapInterval(settings.vsync ? 1 : 0);
        glfwSetWindowUserPointer(window, this);
        glfwSetDropCallback(window, DropCallback);
    } catch(...) {
        context.reset();
        if(window) {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
        throw;
    }
}

WindowSystem::~WindowSystem() {
    context->AssertCurrent();
    glDebugMessageCallback(nullptr, nullptr);
    context.reset();
    glfwDestroyWindow(window);
    glfwTerminate();
}

const GraphicsContext& WindowSystem::GetContext() const {
    return *context;
}

std::pair<int, int> WindowSystem::GetFramebufferSize() const {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    return {width, height};
}

std::vector<std::filesystem::path> WindowSystem::TakeDroppedFiles() {
    std::vector<std::filesystem::path> events;
    events.swap(droppedFiles);
    return events;
}

bool WindowSystem::ShouldClose() const {
    return glfwWindowShouldClose(window) != 0;
}

void WindowSystem::PollEvents() const {
    glfwPollEvents();
}

void WindowSystem::WaitForEvents(double timeoutSeconds) const {
    glfwWaitEventsTimeout(timeoutSeconds);
}

void WindowSystem::Present() const {
    glfwSwapBuffers(window);
}

void WindowSystem::DropCallback(GLFWwindow* window, int count, const char** paths) noexcept {
    try {
        auto* self = static_cast<WindowSystem*>(glfwGetWindowUserPointer(window));
        if(self) {
            for(int index = 0; index < count; ++index) {
                self->droppedFiles.push_back(PathFromUtf8(paths[index]));
            }
        }
    } catch(...) {
        Log(LogLevel::Error, "Could not enqueue dropped files");
    }
}

void WindowSystem::ErrorCallback(int code, const char* description) noexcept {
    try {
        Log(LogLevel::Error,
            "GLFW " + std::to_string(code) + ": " + (description ? description : "unknown error"));
    } catch(...) {
        Log(LogLevel::Error, "GLFW error (message unavailable)");
    }
}
