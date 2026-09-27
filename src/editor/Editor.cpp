#include "editor/Editor.h"

#include "io/FileParser.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>

void Editor::UpdateGUI() {
    const auto models = Assets().GetModelViews();
    auto selected = std::find_if(models.begin(), models.end(), [&](const auto& model) {
        return selectedModel && model.id == *selectedModel;
    });
    if(selected == models.end()) {
        selectedModel = models.empty() ? std::nullopt : std::optional<ModelId>(models.front().id);
        selected = models.begin();
    }

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Model Editor")) {
        const float frameRate = ImGui::GetIO().Framerate;
        ImGui::Text("%.3f ms/frame (%.1f FPS)",
                    frameRate > 0.0f ? 1000.0f / frameRate : 0.0f,
                    frameRate);
        ImGui::Separator();
        const char* preview =
            selected == models.end() ? "No models" : selected->description.filePath.c_str();
        if(ImGui::BeginCombo("Select Model", preview)) {
            for(const auto& model : models) {
                const bool isSelected = selectedModel && *selectedModel == model.id;
                const std::string label =
                    model.description.filePath + "##" + std::to_string(model.id);
                if(ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedModel = model.id;
                }
                if(isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if(selectedModel) {
            if(auto description = Assets().GetDescription(*selectedModel)) {
                ImGui::Separator();
                ImGui::TextUnformatted("Transform");
                bool changed =
                    ImGui::DragFloat3("Position", glm::value_ptr(description->position), 0.01f);
                changed |=
                    ImGui::DragFloat3("Rotation", glm::value_ptr(description->rotation), 1.0f);
                changed |= ImGui::DragFloat3("Scale", glm::value_ptr(description->scale), 0.01f);
                if(changed) {
                    Assets().UpdateTransform(*selectedModel, *description);
                }
                const auto status =
                    std::find_if(models.begin(), models.end(), [&](const auto& model) {
                        return model.id == *selectedModel;
                    });
                if(status != models.end() && !status->error.empty()) {
                    ImGui::TextWrapped("Load failed: %s", status->error.c_str());
                }
                else if(status != models.end() && !status->loaded) {
                    ImGui::TextUnformatted("Loading...");
                }
                if(ImGui::Button("Remove Model")) {
                    Assets().RemoveModel(*selectedModel);
                    selectedModel.reset();
                }
            }
        }
        else {
            ImGui::TextWrapped("Drop an OBJ file into the window to add a model.");
        }
    }
    ImGui::End();
}
