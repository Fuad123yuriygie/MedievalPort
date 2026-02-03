#include "Application.h"
#include "WindowSystem.h"
#include "FileParser.h"
#include <GLFW/glfw3.h>

Application::Application() {
    window = WindowSystem::GetInstance().GetWindow();
    view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
    control = new Control(window, view);
    shader = new Shader("../res/shaders/Basic");
    skybox = new SkyboxSystem();
    FileParser::GetInstance().LoadSavedFiles();
}

void Application::Run() {
    float lastFrameTime = 0.0f;
    while (!glfwWindowShouldClose(window)) {
        float currentFrameTime = (float)glfwGetTime();
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        control->UpdateCameraMovement(deltaTime);
        UpdateGUI();
        GraphicsContext::renderer->Clear();
        shader->Bind();

        {
            std::lock_guard<std::mutex> lock(FileParser::GetInstance().GetObjVectorMutex());
            for (auto& modelData : FileParser::GetInstance().GetObjVector()) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), modelData.position);
                model = glm::rotate(model, glm::radians(modelData.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, glm::radians(modelData.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::rotate(model, glm::radians(modelData.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, modelData.scale);
                glm::mat4 mvp = GraphicsContext::renderer->GetProjectionMatrix() * view * model;
                shader->SetUniformMat4f("u_MVP", &mvp[0][0]);
                GraphicsContext::renderer->Draw(*modelData.va, *modelData.ib, *modelData.ta);
            }
        }

        glm::mat4 proj = GraphicsContext::renderer->GetProjectionMatrix();
        skybox->Render(view, proj);
        RenderGUI();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void Application::UpdateGUI() {
    // empty
}

void Application::RenderGUI() {
    // empty
}