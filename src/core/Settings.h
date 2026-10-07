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
    // Synchronous GL diagnostics serialize the triggering call with string construction and a
    // logger lock. They are a development/troubleshooting mode, not a release default.
#ifndef NDEBUG
    bool debugOutput = true;
    bool synchronousDebugOutput = true;
#else
    bool debugOutput = false;
    bool synchronousDebugOutput = false;
#endif
    // Alpha-masked materials discard fragments below this coverage. Zero disables masking.
    float alphaCutoff = 0.5f;
    glm::vec4 clearColor{0.08f, 0.10f, 0.14f, 1.0f};
    static constexpr float maxDeltaSeconds = 0.1f;
};

// Hard limits and budgets for the asynchronous import pipeline. Every value is measured in bytes
// or counts of independently owned elements so that queue length never stands in for memory use.
struct ImportLimits {
    // CPU import limits. Oversized inputs are rejected before the expensive allocation.
    std::size_t maxSourceBytes = 64ull * 1024 * 1024;
    std::size_t maxVertices = 8'000'000;
    std::size_t maxTriangles = 8'000'000;
    std::size_t maxMaterials = 4096;
    std::size_t maxEncodedImageBytes = 64ull * 1024 * 1024;
    std::size_t maxDecodedPixels = 64ull * 1024 * 1024;

    // Aggregate backpressure across queued, active and completed import work.
    std::size_t maxPendingBytes = 512ull * 1024 * 1024;
    // Named policy: how many completed CPU imports may be retained for the context thread.
    std::size_t maxCompletedImports = 2;

    // GPU residency and per-frame submission budgets.
    std::size_t maxTextureBytes = 512ull * 1024 * 1024;
    std::size_t maxVertexBufferBytes = 512ull * 1024 * 1024;
    std::size_t maxIndexBufferBytes = 256ull * 1024 * 1024;
    std::size_t maxUploadBytesPerFrame = 64ull * 1024 * 1024;
    double maxUploadMillisecondsPerFrame = 8.0;
};

struct ImportSettings {
    static constexpr int defaultTextureSize = 512;
    int textureSize = defaultTextureSize;
    int textureLayersPerPage = 32;
    ImportLimits limits;
    // Named publish policy: at most this many imports may become renderable per frame. This is a
    // latency bound, not a memory or work bound; the byte/time budgets above are the real limits.
    std::size_t uploadsPerFrame = 1;
};
