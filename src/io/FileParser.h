#pragma once

#include "io/LoadData.h"

#include <filesystem>
#include <glm/mat4x4.hpp>
#include <memory>
#include <optional>
#include <vector>

class GraphicsContext;
struct DrawItem;

struct ModelView {
    ModelId id = 0;
    ModelDescription description;
    bool loaded = false;
    std::string error;
};

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
    std::vector<ModelView> GetModelViews() const;
    std::optional<ModelDescription> GetDescription(ModelId id) const;
    bool UpdateTransform(ModelId id, const ModelDescription& description);
    bool RemoveModel(ModelId id);
    std::vector<DrawItem> BuildDrawList(const glm::mat4& viewProjection) const;
    bool SaveScene() const;
    void Shutdown() noexcept;

private:
    bool QueueModelLoad(ModelDescription description);
    struct State;
    std::unique_ptr<State> state;
};
