#pragma once

#include "core/Settings.h"

#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

struct GLFWwindow;
class GraphicsContext;

class WindowSystem {
public:
    explicit WindowSystem(const RenderSettings& settings = {});
    ~WindowSystem();
    WindowSystem(const WindowSystem&) = delete;
    WindowSystem& operator=(const WindowSystem&) = delete;

    GLFWwindow* GetWindow() const {
        return window;
    }
    const GraphicsContext& GetContext() const;
    std::pair<int, int> GetFramebufferSize() const;
    std::vector<std::filesystem::path> TakeDroppedFiles();
    bool ShouldClose() const;
    void PollEvents() const;
    void WaitForEvents(double timeoutSeconds) const;
    void Present() const;

private:
    GLFWwindow* window = nullptr;
    std::unique_ptr<GraphicsContext> context;
    std::vector<std::filesystem::path> droppedFiles;

    static void DropCallback(GLFWwindow* window, int count, const char** paths) noexcept;
    static void ErrorCallback(int code, const char* description) noexcept;
};
