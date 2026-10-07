#pragma once

#include "core/Application.h"
#include "io/SceneTypes.h"

#include <optional>
#include <string>

class Editor final : public Application {
public:
    using Application::Application;

protected:
    void UpdateGUI() override;

private:
    std::optional<ModelId> selectedModel;
    std::string saveStatus;
};
