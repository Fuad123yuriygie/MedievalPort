#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Variables to track mouse movement and camera rotation
static bool rightMouseButtonPressed = false;
static double lastMouseX = 0.0, lastMouseY = 0.0;
static float yaw = -90.0f; // Horizontal rotation (yaw)
static float pitch = 0.0f; // Vertical rotation (pitch)
static float sensitivity = 0.1f; // Mouse sensitivity

// Variables for camera control
static glm::vec3 cameraPosition(0.0f, 0.0f, 3.0f); // Initial camera position
static glm::vec3 cameraFront(0.0f, 0.0f, -1.0f);   // Initial camera direction
static glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);       // Up vector
static float cameraSpeed = 2.5f;                   // Camera movement speed (units per second)

class Control {
public:
    GLFWwindow* window; // Pointer to the GLFW window
    glm::mat4& view;

    Control(GLFWwindow* win, glm::mat4& view);
    ~Control() = default;

    void UpdateCameraMovement(float deltaTime);
    
    private:
    // Function to process keyboard input for camera movement
    void ProcessKeyboardInput(float deltaTime);
    
    // Function to update camera direction based on mouse movement
    void UpdateCameraDirection();
};
