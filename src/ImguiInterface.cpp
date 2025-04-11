#include "ImguiInterface.h"

// Initialize ImGui
ImguiInterface::ImguiInterface(GLFWwindow* window)
    : io(ImguiIoContext()) {
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

// Draw ImGui new frame
void ImguiInterface::ImguiNewFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

// Draw ImGui elemnets
// Make this so that it can take arbitrary number of elements
void ImguiInterface::ImguiFrameElement(float& rotationX, float& rotationY, float& rotationZ) {
    ImGui::Begin("Object Rotation");
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::SliderFloat("Rotation X", &rotationX, 0.0f, 360.0f); // Control rotation around X-axis
    ImGui::SliderFloat("Rotation Y", &rotationY, 0.0f, 360.0f); // Control rotation around Y-axis
    ImGui::SliderFloat("Rotation Z", &rotationZ, 0.0f, 360.0f); // Control rotation around Z-axis
    ImGui::End();
}

// Render ImGui elements
void ImguiInterface::ImguiRender() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
// Shutdown ImGui
ImguiInterface::~ImguiInterface() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

ImGuiIO& ImguiInterface::ImguiIoContext() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    return ImGui::GetIO();
}