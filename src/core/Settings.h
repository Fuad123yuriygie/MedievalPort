#pragma once

#include <cstddef>
#include <glm/vec4.hpp>

struct CameraSettings {
    float fieldOfViewDegrees = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float movementSpeed = 2.5f;
    float mouseSensitivity = 0.1f;
    float maxPitchDegrees = 89.0f;
};

struct RenderSettings {
    int width = 1280;
    int height = 720;
    int samples = 4;
    bool vsync = true;
    bool visible = true;
    glm::vec4 clearColor{0.08f, 0.10f, 0.14f, 1.0f};
    static constexpr float maxDeltaSeconds = 0.1f;
};

struct ImportSettings {
    static constexpr int defaultTextureSize = 512;
    int textureSize = defaultTextureSize;
    int textureLayersPerPage = 32;
    std::size_t uploadsPerFrame = 1;
};
