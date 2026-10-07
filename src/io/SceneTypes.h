#pragma once

// Foundational, dependency-light scene types shared by the importer, persistence, scene math and
// the renderer. This header deliberately avoids OpenGL and process-local handles so that the CPU
// half of the engine stays testable without a graphics context.

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

// How the alpha channel of a material is interpreted. Opaque and Mask can be drawn in any order
// with depth writes enabled; Blend requires back-to-front ordering and depth writes disabled.
enum class AlphaMode : std::uint8_t {
    Opaque,
    Mask,
    Blend
};

// Coverage below this value is discarded for masked materials.
inline constexpr float DefaultAlphaCutoff = 0.5f;

// The subset of the material contract the renderer actually honors. Authored properties that are
// not represented here are reported as unsupported during import rather than silently discarded.
struct MaterialData {
    glm::vec3 diffuse{1.0f, 1.0f, 1.0f};
    float opacity = 1.0f;
    glm::vec2 textureScale{1.0f, 1.0f};
    glm::vec2 textureOffset{0.0f, 0.0f};
    bool clampToEdge = false;
    std::uint32_t textureIndex = 0;
    AlphaMode alphaMode = AlphaMode::Opaque;
};

struct SubmeshData {
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t textureIndex = 0;
    std::uint32_t materialIndex = 0;
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<SubmeshData> submeshes;
    std::vector<MaterialData> materials;
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

// Estimated resident size of a completed import, used for byte-based backpressure.
std::size_t PendingModelBytes(const PendingModelData& model);

// Thrown when a cooperative cancellation is observed mid-import. The worker discards the partial
// result instead of publishing it.
struct ImportCancelled {};
