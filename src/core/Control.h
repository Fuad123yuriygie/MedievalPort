#pragma once

#include <glm/vec3.hpp>

struct GLFWwindow;
class Camera;

struct InputState {
    glm::vec3 movement{0.0f};
    double mouseX = 0.0;
    double mouseY = 0.0;
    bool rightMouseDown = false;
    bool focused = false;
};

class Control {
public:
    explicit Control(GLFWwindow* window);
    ~Control();
    Control(const Control&) = delete;
    Control& operator=(const Control&) = delete;

    void Update(Camera& camera, float deltaSeconds, bool captureMouse, bool captureKeyboard);

private:
    InputState PollInput() const;

    GLFWwindow* window;
    bool dragging = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
};
