#pragma once

#include "core/Settings.h"
#include "io/ImageData.h"

#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

using ModelId = std::uint64_t;

struct ModelDescription {
    std::string filePath; // UTF-8, never an OS-specific narrow code page.
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
};

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct Bounds {
    glm::vec3 minimum{0.0f};
    glm::vec3 maximum{0.0f};
};

struct SubmeshData {
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t textureIndex = 0;
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<SubmeshData> submeshes;
    Bounds bounds;
};

// This is the entire worker/render-thread boundary: no GL types or process-local handles.
struct PendingModelData {
    ModelId requestId = 0;
    ModelDescription description;
    MeshData mesh;
    std::vector<DecodedImage> textures;
    std::string error;

    bool Success() const {
        return error.empty() && !mesh.indices.empty();
    }
};

PendingModelData LoadModelData(ModelDescription description, const ImportSettings& settings = {});
