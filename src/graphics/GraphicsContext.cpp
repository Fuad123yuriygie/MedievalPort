#include "graphics/GraphicsContext.h"

#include <GLFW/glfw3.h>
#include <cassert>
#include <stdexcept>

GraphicsContext::GraphicsContext(GLFWwindow* window)
    : window(window), ownerThread(std::this_thread::get_id()) {
    RequireCurrent();
}

void GraphicsContext::RequireCurrent() const {
    if(std::this_thread::get_id() != ownerThread || glfwGetCurrentContext() != window || !window) {
        throw std::logic_error("OpenGL access requires the owning thread and current context");
    }
}

void GraphicsContext::AssertCurrent() const noexcept {
    assert(std::this_thread::get_id() == ownerThread && window &&
           glfwGetCurrentContext() == window && "OpenGL resource outlived its current context");
}
