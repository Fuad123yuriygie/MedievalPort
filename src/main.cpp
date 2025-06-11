#include <GLFW/glfw3.h>
#include <glad/glad.h>
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
#include "LoadModel.h"
#include "Renderer.h"
#include "Shader.h"
#include "VertexArray.h"
#include "VertexBuffer.h"
#include "WindowSystem.h"

using namespace std::chrono;

int main() {
    FileParser fileParser;
    WindowSystem windowSystem(fileParser);
    GLFWwindow* window = windowSystem.GetWindow();
    // Load GLAD
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    std::cout << glGetString(GL_VERSION) << std::endl;

#ifdef DEBUG
    DebugMessage debugMessage;
#endif

    // Create MVP matrices
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                            (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT,
                                            0.1f,
                                            100.0f);

    Renderer renderer(projection);

    Control control(window, view);

    ImguiInterface imgui(window);

    windowSystem.SetRenderer(renderer);

    // Load the OBJ files
    fileParser.LoadSavedFiles();

    // Bind the shader program
    Shader shader("../res/shaders/Basic.shader");
    shader.Bind();

    // Load texture
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load the texture image
    int width, height, nrChannels;
    // stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load("../res/textures/monkey.png", &width, &height, &nrChannels, 0);
    std::cout << width << " " << height << " " << nrChannels << std::endl;
    if(data) {
        // Determine the format based on the number of channels
        int channels = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     channels,
                     width,
                     height,
                     0,
                     channels,
                     GL_UNSIGNED_BYTE,
                     data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else {
        std::cout << "Failed to load texture" << std::endl;
    }
    stbi_image_free(data);

    float lastFrameTime = 0.0f;
    int selectedModel = 0;

    // Light and camera positions
    glm::vec3 lightPos(1.2f, 1.0f, 2.0f);
    shader.SetUniform3f("u_LightPos", lightPos.x, lightPos.y, lightPos.z);
    shader.SetUniform3f("u_LightColor", 1.0f, 1.0f, 1.0f);

    // Render loop
    while(!glfwWindowShouldClose(window)) {
        float currentFrameTime = (float)glfwGetTime();
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        // Process input
        control.UpdateCameraMovement(deltaTime);

        renderer.Clear();

        // Start ImGui frame
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

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        // Render all models with their own transform
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
            glm::mat4 normalMatrix = glm::transpose(glm::inverse(modelMat));

            shader.SetUniformMat4f("u_Model", glm::value_ptr(modelMat));
            shader.SetUniformMat4f("u_NormalMat", glm::value_ptr(normalMatrix));
            shader.SetUniformMat4f("u_MVP", glm::value_ptr(mvp));
            // shader.SetUniform1i("u_Texture", 0);

            renderer.Draw(*(modelData.va), *(modelData.ib));
        }

        // Render ImGui
        imgui.ImguiRender();

        glfwSwapBuffers(window); // Move this to window system (Future note)
        glfwPollEvents();
    }

    glfwTerminate(); // Make seperate function/namespace/class for glfw codes (Future)
    return 0;
}
