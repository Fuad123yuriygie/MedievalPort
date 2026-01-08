#pragma once

struct GLFWwindow;

class IGUI {
public:
    virtual ~IGUI() = default;
    virtual void Init(GLFWwindow* window) = 0;
    virtual void NewFrame() = 0;
    virtual void FrameElement(float& rotationX, float& rotationY, float& rotationZ) = 0;
    virtual void Render() = 0;
};