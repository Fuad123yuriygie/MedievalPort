#include "ui/ImguiInterface.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <stdexcept>

ImguiInterface::ImguiInterface(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    // Scene persistence is explicit; do not create an unrelated imgui.ini in the working directory.
    ImGui::GetIO().IniFilename = nullptr;
    if(!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        ImGui::DestroyContext();
        throw std::runtime_error("Failed to initialize the ImGui GLFW backend");
    }
    if(!ImGui_ImplOpenGL3_Init("#version 450 core")) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("Failed to initialize the ImGui OpenGL backend");
    }
}

ImguiInterface::~ImguiInterface() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImguiInterface::NewFrame() const {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImguiInterface::Render() const {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool ImguiInterface::WantsMouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImguiInterface::WantsKeyboard() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}
