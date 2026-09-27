#include "core/Control.h"

#include "core/Camera.h"

#include <GLFW/glfw3.h>
#include <stdexcept>

Control::Control(GLFWwindow* window) : window(window) {
    if(!window) {
        throw std::invalid_argument("Camera input requires a window");
    }
}

Control::~Control() {
    if(dragging) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

InputState Control::PollInput() const {
    InputState input;
    input.focused = glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
    input.rightMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    glfwGetCursorPos(window, &input.mouseX, &input.mouseY);
    const auto down = [this](int key) {
        return glfwGetKey(window, key) == GLFW_PRESS ? 1.0f : 0.0f;
    };
    input.movement = {down(GLFW_KEY_D) - down(GLFW_KEY_A),
                      down(GLFW_KEY_E) - down(GLFW_KEY_Q),
                      down(GLFW_KEY_W) - down(GLFW_KEY_S)};
    return input;
}

void Control::Update(Camera& camera, float deltaSeconds, bool captureMouse, bool captureKeyboard) {
    const InputState input = PollInput();
    const bool shouldDrag = input.focused && input.rightMouseDown && (dragging || !captureMouse);
    if(shouldDrag != dragging) {
        dragging = shouldDrag;
        glfwSetInputMode(window, GLFW_CURSOR, dragging ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
    }
    else if(dragging) {
        camera.Look(static_cast<float>(input.mouseX - lastMouseX),
                    static_cast<float>(lastMouseY - input.mouseY));
        lastMouseX = input.mouseX;
        lastMouseY = input.mouseY;
    }
    if(input.focused && !captureKeyboard) {
        camera.Move(input.movement, deltaSeconds);
    }
}
