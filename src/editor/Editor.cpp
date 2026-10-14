#include "editor/Editor.h"

#include "io/FileParser.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

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
    ImGui::SetNextWindowSize(ImVec2(380, 650), ImGuiCond_FirstUseEver);
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
        if(ImGui::CollapsingHeader("Scene Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto lighting = Assets().GetLighting();
            bool changed = ImGui::Checkbox("Enable Lighting", &lighting.enabled);
            changed |= ImGui::DragFloat3("Ray Direction", glm::value_ptr(lighting.direction), 0.01f);
            ImGui::TextWrapped("World-space direction rays travel; (0, -1, 0) shines down.");
            changed |= ImGui::ColorEdit3("Sun Color (Linear)", glm::value_ptr(lighting.color),
                                         ImGuiColorEditFlags_Float);
            changed |= ImGui::SliderFloat("Sun Intensity", &lighting.intensity, 0.0f, 4.0f);
            changed |= ImGui::SliderFloat("Ambient Strength", &lighting.ambientStrength, 0.0f, 1.0f);
            if(ImGui::Button("Reset Lighting")) {
                lighting = SceneLighting{};
                changed = true;
            }
            if(changed) {
                lightingEditRejected = !Assets().UpdateLighting(lighting);
            }
            if(lightingEditRejected) {
                ImGui::TextWrapped("Edit rejected; previous lighting kept. Use a finite, nonzero "
                                   "direction, color 0..1, intensity 0..4, and ambient 0..1.");
            }
        }
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
