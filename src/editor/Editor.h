#pragma once

#include "core/Application.h"
#include "io/LoadData.h"

#include <optional>

class Editor final : public Application {
public:
    using Application::Application;

protected:
    void UpdateGUI() override;

private:
    std::optional<ModelId> selectedModel;
};
