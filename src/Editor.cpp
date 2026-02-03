#include "Editor.h"
#include "GraphicsContext.h"
#include <glm/gtc/type_ptr.hpp>
#include "ImguiInterface.h"
#include "FileParser.h"
#include <imgui/imgui.h>

Editor::Editor() {
    // init if needed
}

void Editor::UpdateGUI() {
    GraphicsContext::gui->NewFrame();

    ImGui::Begin("Model Editor");
    auto& fileParser = FileParser::GetInstance();
    
    // Lock the mutex to safely access model data
    std::lock_guard<std::mutex> lock(fileParser.GetObjVectorMutex());
    
    if (!fileParser.GetModelNames().empty()) {
        ImGui::Combo("Select Model", &selectedModel, fileParser.GetModelNames().data(), (int)fileParser.GetModelNames().size());
        ImGui::Separator();
        ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Separator();
        ImGui::Text("Transform");
        ImGui::DragFloat3("Position", glm::value_ptr(fileParser.GetObjVector()[selectedModel].position), 0.01f);
        ImGui::DragFloat3("Rotation", glm::value_ptr(fileParser.GetObjVector()[selectedModel].rotation), 1.0f);
        ImGui::DragFloat3("Scale", glm::value_ptr(fileParser.GetObjVector()[selectedModel].scale), 0.01f, 0.01f, 100.0f);
    }
    ImGui::End();
}

void Editor::RenderGUI() {
    GraphicsContext::gui->Render();
}