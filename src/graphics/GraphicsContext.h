#pragma once

#include <thread>

struct GLFWwindow;

class GraphicsContext {
public:
    explicit GraphicsContext(GLFWwindow* window);
    GraphicsContext(const GraphicsContext&) = delete;
    GraphicsContext& operator=(const GraphicsContext&) = delete;

    void RequireCurrent() const;
    void AssertCurrent() const noexcept;

private:
    GLFWwindow* window;
    std::thread::id ownerThread;
};
