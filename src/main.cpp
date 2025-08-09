#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Control.h"
#include "DebugMessage.h"
#include "FileParser.h"
#include "ImguiInterface.h"
#include "IndexBuffer.h"
#include "LoadData.h"
#include "Renderer.h"
#include "Shader.h"
#include "VertexArray.h"
#include "VertexBuffer.h"
#include "WindowSystem.h"
#include "SkyboxSystem.h"

int main() {
    FileParser fileParser;
    WindowSystem windowSystem(fileParser);
    GLFWwindow* window = windowSystem.GetWindow();

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    std::cout << glGetString(GL_VERSION) << std::endl;

#ifdef DEBUG
    DebugMessage debugMessage;
#endif

    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 cameraView = glm::vec3(0.0f, 0.0f, -3.0f);
    glm::mat4 view = glm::translate(glm::mat4(1.0f), cameraView);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                            (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT,
                                            0.1f,
                                            100.0f);

    Renderer renderer(projection);
    Control control(window, view);
    ImguiInterface imgui(window);

    windowSystem.SetRenderer(renderer);

    fileParser.LoadSavedFiles();

    // Load the shaders from render class (actually the render class should get the shader from model class, that is not yet been impremented)
    Shader shader("../res/shaders/Basic");
    shader.Bind();

    SkyboxSystem skyboxSystem;
    
    float lastFrameTime = 0.0f;
    int selectedModel = 0;

    // Render loop
    while(!glfwWindowShouldClose(window)) {
        float currentFrameTime = (float)glfwGetTime();
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        // Process input
        control.UpdateCameraMovement(deltaTime);
        renderer.Clear();
        shader.Bind();
        imgui.ImguiNewFrame();

        // ImGui window for rotation controls
        if(!fileParser.modelNames.empty()) {
            ImGui::Combo("Select Model",
                         &selectedModel,
                         fileParser.modelNames.data(),
                         (int)fileParser.modelNames.size());

            // Controls for selected model
            ImGui::Separator();
            ImGui::Text("%.3f ms/frame (%.1f FPS)",
                        1000.0f / imgui.io.Framerate,
                        imgui.io.Framerate);
            ImGui::Separator();
            ImGui::Text("Transform");
            ImGui::DragFloat3("Position",
                              glm::value_ptr(fileParser.objVector[selectedModel].position),
                              0.01f);
            ImGui::DragFloat3("Rotation",
                              glm::value_ptr(fileParser.objVector[selectedModel].rotation),
                              1.0f);
            ImGui::DragFloat3("Scale",
                              glm::value_ptr(fileParser.objVector[selectedModel].scale),
                              0.01f,
                              0.01f,
                              100.0f);
        }

        for(auto& modelData : fileParser.objVector) {
            glm::mat4 modelMat = glm::mat4(1.0f);
            modelMat = glm::translate(modelMat, modelData.position);
            modelMat =
                glm::rotate(modelMat, glm::radians(modelData.rotation.x), glm::vec3(1, 0, 0));
            modelMat =
                glm::rotate(modelMat, glm::radians(modelData.rotation.y), glm::vec3(0, 1, 0));
            modelMat =
                glm::rotate(modelMat, glm::radians(modelData.rotation.z), glm::vec3(0, 0, 1));
            modelMat = glm::scale(modelMat, modelData.scale);

            glm::mat4 mvp = projection * view * modelMat;
            shader.SetUniformMat4f("u_MVP", glm::value_ptr(mvp));

            renderer.Draw(*(modelData.va), *(modelData.ib), *(modelData.ta));
        }
        
        skyboxSystem.Render(view, projection);

        imgui.ImguiRender();

        glfwSwapBuffers(window); // Move this to window system (Future note)
        glfwPollEvents();
    }

    return 0;
}
