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

#include "Renderer.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "Shader.h"
#include "Control.h"
#include "LoadModel.h"
#include "ImguiInterface.h"

// Variables for rotation angles
float rotationX = 0.0f; // Rotation around X-axis
float rotationY = 0.0f; // Rotation around Y-axis
float rotationZ = 0.0f; // Rotation around Z-axis

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

int main() {
    GLFWwindow* window;

    // Init library
    if (!glfwInit()) {
        return -1;
    }

    // Request an OpenGL debug context
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    // Create windowed mode window and its OpenGL context
    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
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

    // Create MVP matrices
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 640.0f / 480.0f, 0.1f, 100.0f);

    Renderer renderer; // Create a Renderer instance

    Control control(window, view); // Create a Control instance
    
    ImguiInterface imgui(window); // Create an ImguiInterface instance

    // Load the OBJ file
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    if (!LoadObject("../res/models/monkey.obj", vertices, indices)) {
        std::cout << "Failed to load OBJ file" << std::endl;
        return -1; // Exit if the OBJ file fails to load
    }

    // Create OpenGL buffers for the loaded OBJ data
    VertexArray va;
    VertexBuffer vb(vertices.data(), vertices.size() * sizeof(float));

    VertexBufferLayout layout;
    layout.Push<float>(3); // Position (x, y, z)
    layout.Push<float>(3); // Normal (nx, ny, nz)
    layout.Push<float>(2); // Texture coordinates (u, v)
    va.AddBuffer(vb, layout);

    IndexBuffer ib(indices.data(), indices.size());

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

    float lastFrameTime = 0.0f;

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        // Calculate delta time
        // There are two ways delta time can be serverd
        // 1. By the renderer
        // 2. By the control
        // 3. Or the window constructor? (Future)
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
        shader.SetUniform3f("u_ViewPos", viewPos.x, viewPos.y, viewPos.z);
        shader.SetUniform3f("u_LightColor", 1.0f, 0.0f, 1.0f); // White light
        shader.SetUniform3f("u_ObjectColor", 1.0f, 0.5f, 0.31f); // Object color

        // Pass model, view, and projection matrices
        glm::mat4 normalMatrix = glm::transpose(glm::inverse(model));
        shader.SetUniformMat4f("u_Model", glm::value_ptr(model));
        shader.SetUniformMat4f("u_NormalMat", glm::value_ptr(normalMatrix));
        shader.SetUniformMat4f("u_MVP", glm::value_ptr(mvp));

        shader.Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        shader.SetUniform1i("u_Texture", 0);
        renderer.Draw(va, ib);
        
        // Render ImGui
        imgui.ImguiRender(); // Render ImGui elements

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate(); // Make seperate function/namespace/class for glfw codes
    return 0;
}
