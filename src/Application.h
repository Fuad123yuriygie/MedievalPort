#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Control.h"
#include "Shader.h"
#include "SkyboxSystem.h"
#include "GraphicsContext.h"

class Application {
public:
    Application();
    void Run();
protected:
    virtual void UpdateGUI();
    virtual void RenderGUI();
private:
    GLFWwindow* window;
    glm::mat4 view;
    Control* control;
    Shader* shader;
    SkyboxSystem* skybox;
};