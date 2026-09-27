#pragma once

struct GLFWwindow;

class ImguiInterface {
public:
    explicit ImguiInterface(GLFWwindow* window);
    ~ImguiInterface();
    ImguiInterface(const ImguiInterface&) = delete;
    ImguiInterface& operator=(const ImguiInterface&) = delete;

    void NewFrame() const;
    void Render() const;
    bool WantsMouse() const;
    bool WantsKeyboard() const;
};
