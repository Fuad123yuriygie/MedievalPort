#pragma once

#include "Application.h"

class Editor : public Application {
public:
    Editor();
protected:
    void UpdateGUI() override;
    void RenderGUI() override;
private:
    int selectedModel = 0;
};