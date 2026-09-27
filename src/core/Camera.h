#pragma once

#include "core/Settings.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

class Camera {
public:
    explicit Camera(const CameraSettings& settings = {});

    void Resize(int width, int height);
    void Move(const glm::vec3& localDirection, float deltaSeconds);
    void Look(float horizontalPixels, float verticalPixels);
    const glm::mat4& GetView() const {
        return view;
    }
    const glm::mat4& GetProjection() const {
        return projection;
    }
    const glm::vec3& GetPosition() const {
        return position;
    }

private:
    void UpdateView();

    CameraSettings settings;
    glm::vec3 position{0.0f, 0.0f, 3.0f};
    glm::vec3 front{0.0f, 0.0f, -1.0f};
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    float yawDegrees = -90.0f;
    float pitchDegrees = 0.0f;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
};
