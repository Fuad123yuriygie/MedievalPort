#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include "IGUI.h"

class ImguiInterface : public IGUI {
public:
    ImguiInterface();
    ~ImguiInterface();
    void Init(GLFWwindow* window) override;
    void NewFrame() override { ImguiNewFrame(); }
    void FrameElement(float& rotationX, float& rotationY, float& rotationZ) override { ImguiFrameElement(rotationX, rotationY, rotationZ); }
    void Render() override { ImguiRender(); }
    void ImguiNewFrame();
    void ImguiFrameElement(float& rotationX, float& rotationY, float& rotationZ);
    void ImguiRender();
    ImGuiIO& io;

private:
    ImGuiIO& ImguiIoContext();
};