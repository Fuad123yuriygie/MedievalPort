#include "Control.h"

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if(button == GLFW_MOUSE_BUTTON_RIGHT) {
        if(action == GLFW_PRESS) {
            rightMouseButtonPressed = true;
            glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
        }
        else if(action == GLFW_RELEASE) {
            rightMouseButtonPressed = false;
        }
    }
}

// Callback to track window focus
static void WindowFocusCallback(GLFWwindow* window, int focused) {
    if(!focused) {
        rightMouseButtonPressed = false;
    }
}

// Constructor to initialize the Control class with a GLFW window pointer
Control::Control(GLFWwindow* win, glm::mat4& view) : window(win), view(view) {
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetWindowFocusCallback(window, WindowFocusCallback);
}

void Control::UpdateCameraMovement(float deltaTime) {
    ProcessKeyboardInput(deltaTime);
    UpdateCameraDirection();

    // Update the view matrix based on the camera's position and direction
    view = glm::lookAt(cameraPosition, cameraPosition + cameraFront, cameraUp);
}

// Function to process keyboard input for camera movement
void Control::ProcessKeyboardInput(float deltaTime) {
    float velocity = cameraSpeed * deltaTime;
    if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        cameraPosition += velocity * cameraFront;
    }
    if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        cameraPosition -= velocity * cameraFront;
    }
    if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        cameraPosition -= glm::normalize(glm::cross(cameraFront, cameraUp)) * velocity;
    }
    if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        cameraPosition +=
            glm::normalize(glm::cross(cameraFront, cameraUp)) * velocity;
    }
    if(glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        cameraPosition += cameraUp * velocity;
    }
    if(glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        cameraPosition -= cameraUp * velocity;
    }
}

// Function to update camera direction based on mouse movement
void Control::UpdateCameraDirection() {
    if(rightMouseButtonPressed) {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        float xOffset = (float)(mouseX - lastMouseX) * sensitivity;
        float yOffset = (float)(lastMouseY - mouseY) * sensitivity;

        lastMouseX = mouseX;
        lastMouseY = mouseY;

        yaw += xOffset;
        pitch += yOffset;

        // Constrain pitch to avoid flipping
        if(pitch > 89.0f)
            pitch = 89.0f;
        if(pitch < -89.0f)
            pitch = -89.0f;

        // Calculate the new camera direction
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(front);
    }
}
