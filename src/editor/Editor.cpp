#include "editor/Editor.h"

#include "io/FileParser.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>

void Editor::UpdateGUI() {
    // Cached snapshot: rebuilt only when scene state changes, not on every frame.
    const auto& models = Assets().GetModelViews();
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
        // Explicit persistence while the application is still fully operational.
        if(ImGui::Button("Save Scene")) {
            if(Assets().SaveScene()) {
                saveStatus = "Scene saved";
            }
            else {
                saveStatus = "Save failed - previous file kept";
            }
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(Assets().IsDirty() ? "Unsaved changes"
                                                  : (saveStatus.empty() ? "Saved" : saveStatus.c_str()));
        ImGui::Separator();
        const char* preview =
            selected == models.end() ? "No models" : selected->description.filePath.c_str();
        if(ImGui::BeginCombo("Select Model", preview)) {
            for(const auto& model : models) {
                const bool isSelected = selectedModel && *selectedModel == model.id;
                if(ImGui::Selectable(model.label.c_str(), isSelected)) {
                    selectedModel = model.id;
                }
                if(isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if(selectedModel) {
            if(selected != models.end()) {
                ImGui::Separator();
                ImGui::TextUnformatted("Transform");
                // Edit from the snapshot's transforms; no file path is copied to change a transform.
                glm::vec3 position = selected->description.position;
                glm::vec3 rotation = selected->description.rotation;
                glm::vec3 scale = selected->description.scale;
                bool changed = ImGui::DragFloat3("Position", glm::value_ptr(position), 0.01f);
                changed |= ImGui::DragFloat3("Rotation", glm::value_ptr(rotation), 1.0f);
                changed |= ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.01f);
                if(changed) {
                    Assets().UpdateTransform(*selectedModel, position, rotation, scale);
                }
                if(!selected->error.empty()) {
                    ImGui::TextWrapped("Load failed: %s", selected->error.c_str());
                }
                else if(!selected->loaded) {
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
