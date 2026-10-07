#pragma once

#include "io/LoadData.h"
#include "io/SceneTypes.h"

#include <filesystem>
#include <glm/mat4x4.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class GraphicsContext;
struct DrawItem;

// Immutable, frame-scoped description of a scene instance for UI consumption.
struct ModelView {
    ModelId id = 0;
    ModelDescription description;
    std::string label; // Cached combo label ("<path>##<id>"), so the UI does not rebuild it.
    bool loaded = false;
    std::string error;
};

// Composition root for scene instances. Owns the asynchronous importer, GPU asset cache and
// texture residency, and exposes import, submission and persistence operations. Resource lifetimes
// live in TextureResidency and the mesh cache rather than in individual scene instances.
class FileParser {
public:
    FileParser(const GraphicsContext& context, std::filesystem::path sceneFile,
               const ImportSettings& settings = {});
    ~FileParser();
    FileParser(const FileParser&) = delete;
    FileParser& operator=(const FileParser&) = delete;

    bool LoadAsset(const std::filesystem::path& path);
    void LoadSavedFiles();
    void ProcessPendingModels();

    // Rebuilt only when scene state changes; the editor may hold the reference for a frame.
    const std::vector<ModelView>& GetModelViews() const;
    std::optional<ModelDescription> GetDescription(ModelId id) const;
    bool UpdateTransform(ModelId id, const glm::vec3& position, const glm::vec3& rotation,
                         const glm::vec3& scale);
    bool UpdateTransform(ModelId id, const ModelDescription& description);
    bool RemoveModel(ModelId id);

    // Writes into caller-owned storage so frame processing does not allocate a fresh draw vector.
    void BuildDrawList(const glm::mat4& viewProjection, const glm::vec3& cameraPosition,
                       std::vector<DrawItem>& out) const;
    std::vector<DrawItem> BuildDrawList(const glm::mat4& viewProjection) const;

    // Explicit persistence. SaveScene clears the dirty flag only when the write succeeds.
    bool SaveScene() const;
    bool IsDirty() const;
    void Shutdown() noexcept;

private:
    bool QueueModelLoad(ModelDescription description);
    struct State;
    std::unique_ptr<State> state;
};
