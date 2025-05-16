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
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <nlohmann/json.hpp>

#include "Renderer.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "Shader.h"
#include "Control.h"
#include "LoadModel.h"
#include "ImguiInterface.h"

// Alias for JSON library
using json = nlohmann::json;

// Variables for rotation angles
float rotationX = 0.0f; // Rotation around X-axis
float rotationY = 0.0f; // Rotation around Y-axis
float rotationZ = 0.0f; // Rotation around Z-axis

// Obj vector to store the loaded model data
struct ModelData {
    VertexArray* va;
    IndexBuffer* ib;
    VertexBuffer* vb;

    // ~ModelData() {
    //     delete va;
    //     delete ib;
    // }
};
std::vector<ModelData> objVector;

// Function to save rotation values to a JSON file
void SaveRotationValues(float rotationX, float rotationY, float rotationZ) {
    json rotationData = {
        {"rotationX", rotationX},
        {"rotationY", rotationY},
        {"rotationZ", rotationZ}
    };

    std::ofstream outFile("rotation_config.json");
    if (outFile.is_open()) {
        outFile << rotationData.dump(4); // Pretty print with 4 spaces
        outFile.close();
    } else {
        std::cerr << "Failed to save rotation values to file." << std::endl;
    }
}

// Function to load rotation values from a JSON file
void LoadRotationValues(float& rotationX, float& rotationY, float& rotationZ) {
    std::ifstream inFile("rotation_config.json");
    if (inFile.is_open()) {
        json rotationData;
        inFile >> rotationData;

        rotationX = rotationData.value("rotationX", 0.0f);
        rotationY = rotationData.value("rotationY", 0.0f);
        rotationZ = rotationData.value("rotationZ", 0.0f);

        inFile.close();
    } else {
        std::cerr << "No rotation configuration file found. Using default values." << std::endl;
    }
}

// Debug callback function
void APIENTRY openglDebugCallback(GLenum source, GLenum type, GLuint id,
                                  GLenum severity, GLsizei length,
                                  const GLchar* message, const void* userParam) {
    std::cerr << "OpenGL Debug Message: " << message << std::endl;
}

// Function to set up the debug callback
void setupDebugCallback() {
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(openglDebugCallback, nullptr);
}

// Drop callback function
void DropCallback(GLFWwindow* window, int count, const char** paths) {
    for (int i = 0; i < count; i++) {
        std::string filePath = paths[i];
        std::cout << "Dropped file: " << filePath << std::endl;

        // Check if the file is an .obj file
        if (filePath.substr(filePath.find_last_of(".") + 1) == "obj") {
            std::vector<float> vertices;
            std::vector<unsigned int> indices;

            // Load the new .obj file
            if (LoadObject(filePath.c_str(), vertices, indices)) {
                std::cout << "Successfully loaded OBJ file: " << filePath << std::endl;

                // Update the OpenGL buffers with the new model data
                VertexArray* va = new VertexArray(); // Declare and initialize the VertexArray object
                VertexBuffer* vb = new VertexBuffer(vertices.data(), vertices.size() * sizeof(float));
                VertexBufferLayout layout;
                layout.Push<float>(3); // Position (x, y, z)
                layout.Push<float>(3); // Normal (nx, ny, nz)
                layout.Push<float>(2); // Texture coordinates (u, v)
                va->AddBuffer(*vb, layout);

                IndexBuffer* ib = new IndexBuffer(indices.data(), indices.size());
                objVector.push_back({ va, ib, vb }); // Store the new model data in the vector
                std::cout << "Model data updated successfully." << std::endl;
            } else {
                std::cerr << "Failed to load OBJ file: " << filePath << std::endl;
            }
        } else {
            std::cerr << "Unsupported file type: " << filePath << std::endl;
        }
    }
}

int main() {
    GLFWwindow* window;

    // Init library
    if (!glfwInit()) {
        return -1;
    }

    // Request an OpenGL debug context
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    // Create windowed mode window and its OpenGL context
    window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Hello World", NULL, NULL);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);
    // glfwSwapInterval(1); // Enable vsync

    // Load GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    std::cout << glGetString(GL_VERSION) << std::endl;

    // Set up OpenGL debug callback
    setupDebugCallback();

    // Set drop callback
    glfwSetDropCallback(window, DropCallback);

    // Create MVP matrices
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 100.0f);

    Renderer renderer; // Create a Renderer instance

    Control control(window, view); // Create a Control instance
    
    ImguiInterface imgui(window); // Create an ImguiInterface instance

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    // Load the OBJ file
    if (!LoadObject("../res/models/monkey.obj", vertices, indices)) {
        std::cout << "Failed to load OBJ file" << std::endl;
        return -1; // Exit if the OBJ file fails to load
    }

    // Create OpenGL buffers for the loaded OBJ data
    VertexArray* va = new VertexArray(); // Create a new VertexArray object
    VertexBuffer* vb = new VertexBuffer(vertices.data(), vertices.size() * sizeof(float));

    VertexBufferLayout layout;
    layout.Push<float>(3); // Position (x, y, z)
    layout.Push<float>(3); // Normal (nx, ny, nz)
    layout.Push<float>(2); // Texture coordinates (u, v)

    va->AddBuffer(*vb, layout); // Add the vertex buffer to the vertex array
    
    IndexBuffer* ib = new IndexBuffer(indices.data(), indices.size());
    objVector.push_back({ va, ib, vb }); // Store the model data in the vector

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
    unsigned char* data = stbi_load("../res/textures/monkey.png", &width, &height, &nrChannels, 0);
    std::cout << width << " " << height << " " << nrChannels << std::endl;
    if (data) {
        int channels = (nrChannels == 4) ? GL_RGBA : GL_RGB; // Determine the format based on the number of channels
        glTexImage2D(GL_TEXTURE_2D, 0, channels, width, height, 0, channels, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cout << "Failed to load texture" << std::endl;
    }
    stbi_image_free(data);

    // Load rotation values from file
    LoadRotationValues(rotationX, rotationY, rotationZ);

    float lastFrameTime = 0.0f;

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrameTime = (float)glfwGetTime();
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        // Process input
        control.UpdateCameraMovement(deltaTime); // Update camera movement based on input

        renderer.Clear();

        // Start ImGui frame
        imgui.ImguiNewFrame(); // Start a new ImGui frame

        // ImGui window for rotation controls
        imgui.ImguiFrameElement(rotationX, rotationY, rotationZ); // Create ImGui elements for rotation

        // Update the model matrix with rotations
        model = glm::mat4(1.0f);
        model = glm::rotate(model, glm::radians(rotationX), glm::vec3(1.0f, 0.0f, 0.0f)); // Rotate around X-axis
        model = glm::rotate(model, glm::radians(rotationY), glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate around Y-axis
        model = glm::rotate(model, glm::radians(rotationZ), glm::vec3(0.0f, 0.0f, 1.0f)); // Rotate around Z-axis

        // Recalculate the MVP matrix
        glm::mat4 mvp = projection * view * model;

        // Light and camera positions
        glm::vec3 lightPos(1.2f, 1.0f, 2.0f); // Light position
        glm::vec3 viewPos(0.0f, 0.0f, 3.0f);  // Camera position

        // Set uniforms for lighting
        shader.Bind();
        shader.SetUniform3f("u_LightPos", lightPos.x, lightPos.y, lightPos.z);
        shader.SetUniform3f("u_LightColor", 1.0f, 0.0f, 0.0f); // White light

        // Pass model, view, and projection matrices
        glm::mat4 normalMatrix = glm::transpose(glm::inverse(model));
        shader.SetUniformMat4f("u_Model", glm::value_ptr(model));
        shader.SetUniformMat4f("u_NormalMat", glm::value_ptr(normalMatrix));
        shader.SetUniformMat4f("u_MVP", glm::value_ptr(mvp));
        shader.SetUniform1i("u_Texture", 0);

        shader.Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        
        for(auto& it: objVector) {
            renderer.Draw(*(it.va), *(it.ib));
        }
        
        // Render ImGui
        imgui.ImguiRender(); // Render ImGui elements

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Save rotation values to file
    SaveRotationValues(rotationX, rotationY, rotationZ);

    glfwTerminate(); // Make seperate function/namespace/class for glfw codes
    return 0;
}
