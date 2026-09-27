#include "core/Camera.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

namespace
{
constexpr glm::vec3 cameraUp{0.0f, 1.0f, 0.0f};
}

Camera::Camera(const CameraSettings& settings) : settings(settings) {
    if(!(settings.fieldOfViewDegrees > 0.0f && settings.fieldOfViewDegrees < 180.0f &&
         settings.nearPlane > 0.0f && settings.farPlane > settings.nearPlane &&
         std::isfinite(settings.farPlane) && settings.movementSpeed >= 0.0f &&
         std::isfinite(settings.movementSpeed) && settings.mouseSensitivity >= 0.0f &&
         std::isfinite(settings.mouseSensitivity) && settings.maxPitchDegrees > 0.0f &&
         settings.maxPitchDegrees < 90.0f)) {
        throw std::invalid_argument("Invalid camera settings");
    }
    Resize(1, 1);
    UpdateView();
}

void Camera::Resize(int width, int height) {
    if(width <= 0 || height <= 0 || (width == framebufferWidth && height == framebufferHeight)) {
        return;
    }
    projection = glm::perspective(glm::radians(settings.fieldOfViewDegrees),
                                  static_cast<float>(width) / static_cast<float>(height),
                                  settings.nearPlane,
                                  settings.farPlane);
    framebufferWidth = width;
    framebufferHeight = height;
}

void Camera::Move(const glm::vec3& localDirection, float deltaSeconds) {
    if(!std::isfinite(deltaSeconds) || glm::dot(localDirection, localDirection) == 0.0f) {
        return;
    }
    const auto right = glm::normalize(glm::cross(front, cameraUp));
    const auto direction =
        right * localDirection.x + cameraUp * localDirection.y + front * localDirection.z;
    const float seconds = std::clamp(deltaSeconds, 0.0f, RenderSettings::maxDeltaSeconds);
    position += glm::normalize(direction) * settings.movementSpeed * seconds;
    UpdateView();
}

void Camera::Look(float horizontalPixels, float verticalPixels) {
    if(!std::isfinite(horizontalPixels) || !std::isfinite(verticalPixels)) {
        return;
    }
    yawDegrees = std::remainder(yawDegrees + horizontalPixels * settings.mouseSensitivity, 360.0f);
    pitchDegrees = std::clamp(pitchDegrees + verticalPixels * settings.mouseSensitivity,
                              -settings.maxPitchDegrees,
                              settings.maxPitchDegrees);
    const float yaw = glm::radians(yawDegrees);
    const float pitch = glm::radians(pitchDegrees);
    front = glm::normalize(glm::vec3(std::cos(yaw) * std::cos(pitch),
                                     std::sin(pitch),
                                     std::sin(yaw) * std::cos(pitch)));
    UpdateView();
}

void Camera::UpdateView() {
    view = glm::lookAt(position, position + front, cameraUp);
}
