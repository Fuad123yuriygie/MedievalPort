#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

class ImguiInterface {
    public:
    ImguiInterface(GLFWwindow* window);
    ~ImguiInterface();
    void ImguiNewFrame();
    void ImguiFrameElement(float& rotationX, float& rotationY, float& rotationZ);
    void ImguiRender();
    private:
    ImGuiIO& io;
    ImGuiIO& ImguiIoContext();
};