#include "io/FileParser.h"

#include "core/SceneMath.h"
#include "graphics/GraphicsContext.h"
#include "graphics/IndexBuffer.h"
#include "graphics/Renderer.h"
#include "graphics/TextureArray.h"
#include "graphics/TextureResidency.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"
#include "graphics/VertexBufferLayout.h"
#include "io/ConfigManager.h"
#include "io/ImageData.h"
#include "io/ModelLoaderThread.h"
#include "io/PathUtils.h"
#include "utils/Log.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <glad/glad.h>
#include <glm/matrix.hpp>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace
{
struct GpuSubmesh {
    SubmeshData range;
    TextureLocation texture;
    MaterialData material;
};

struct GpuMesh {
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    VertexArray vertexArray;
    std::vector<GpuSubmesh> submeshes;
    std::vector<std::string> textureKeys; // cache keys acquired for this mesh
    Bounds bounds;

    GpuMesh(const GraphicsContext& context, const MeshData& mesh,
            std::span<const TextureLocation> textures)
        : vertexBuffer(context, std::as_bytes(std::span(mesh.vertices))),
          indexBuffer(context, mesh.indices), vertexArray(context), bounds(mesh.bounds) {
        static_assert(sizeof(MeshVertex) == sizeof(float) * 8);
        VertexBufferLayout layout;
        layout.Push<float>(0, 3);
        layout.Push<float>(1, 3);
        layout.Push<float>(2, 2);
        vertexArray.AddBuffer(vertexBuffer, layout);
        // The EBO is VAO state, attached deliberately, never rebound during drawing.
        vertexArray.SetIndexBuffer(indexBuffer);
        for(const auto& range : mesh.submeshes) {
            if(range.textureIndex >= textures.size() ||
               static_cast<std::size_t>(range.firstIndex) + range.indexCount >
                   mesh.indices.size()) {
                throw std::invalid_argument("Invalid imported submesh range or texture slice");
            }
            MaterialData material;
            if(range.materialIndex < mesh.materials.size()) {
                material = mesh.materials[range.materialIndex];
            }
            submeshes.push_back({range, textures[range.textureIndex], material});
        }
    }
};

struct SceneModel {
    ModelId id = 0;
    ModelDescription description;
    glm::mat4 modelMatrix{1.0f};
    Bounds localBounds;
    Bounds worldBounds;
    bool mirrored = false;
    std::shared_ptr<GpuMesh> mesh;
    std::string assetKey;
    std::string error;
};

struct FailedAsset {
    std::uint64_t fingerprint = 0;
    std::string error;
};

std::string CanonicalAssetKey(const std::string& filePath) {
    return PathToUtf8(
        std::filesystem::weakly_canonical(std::filesystem::absolute(PathFromUtf8(filePath))));
}

std::uint64_t AssetFingerprint(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if(error) {
        return 0;
    }
    std::uint64_t fingerprint = 0xcbf29ce484222325ull ^ static_cast<std::uint64_t>(size);
    const auto modified = std::filesystem::last_write_time(path, error);
    if(!error) {
        fingerprint ^= static_cast<std::uint64_t>(modified.time_since_epoch().count()) +
                       0x9e3779b97f4a7c15ull + (fingerprint << 6) + (fingerprint >> 2);
    }
    return fingerprint;
}

// Validates and clamps the import settings against the live driver limits before any dependent
// subsystem (texture residency, loader) copies them.
ImportSettings SanitizeImportSettings(const GraphicsContext& context, ImportSettings settings) {
    context.RequireCurrent();
    GLint maxLayers = 0;
    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    if(settings.textureSize <= 0 || settings.textureSize > maxSize ||
       settings.textureLayersPerPage <= 0 || settings.uploadsPerFrame == 0 ||
       settings.limits.maxCompletedImports == 0) {
        throw std::invalid_argument("Invalid texture import or upload budget settings");
    }
    settings.textureLayersPerPage = std::min(settings.textureLayersPerPage, maxLayers);
    return settings;
}
} // namespace

struct FileParser::State {
    const GraphicsContext& context;
    ImportSettings settings;
    std::filesystem::path sceneFile;
    ConfigManager config;
    std::vector<SceneModel> models;
    TextureResidency textures;
    std::unordered_map<std::string, std::shared_ptr<GpuMesh>> meshCache;
    std::unordered_map<std::string, std::uint64_t> meshFingerprints;
    std::unordered_map<std::string, FailedAsset> failedAssets;
    std::unordered_map<std::string, ModelId> pendingAssets; // asset key -> requesting requestId
    std::unordered_map<ModelId, std::string> requestPaths;
    ModelId nextId = 1;
    bool savedFilesLoaded = false;
    bool stopped = false;
    mutable bool dirty = false;
    mutable std::vector<ModelView> viewCache;
    mutable bool viewCacheDirty = true;
    ModelLoaderThread loader;

    enum class UploadOutcome {
        InProgress,
        Finished,
        Failed
    };

    // Incremental upload of one completed import: one texture layer or one buffer set per step.
    struct UploadJob {
        PendingModelData data;
        std::string assetKey;
        std::vector<std::uint32_t> textureOrder;
        std::vector<TextureLocation> locations;
        std::vector<std::string> acquiredKeys;
        std::unique_ptr<GpuMesh> mesh;
        std::size_t nextTexture = 0;
        bool buffersUploaded = false;
    };
    std::optional<UploadJob> uploadJob;

    State(const GraphicsContext& context, std::filesystem::path sceneFile,
          const ImportSettings& settings)
        : context(context), settings(SanitizeImportSettings(context, settings)),
          sceneFile(std::move(sceneFile)), config(this->sceneFile),
          textures(context, this->settings), loader(this->settings) {
    }

    std::size_t CountModels(const std::string& assetKey) const {
        return static_cast<std::size_t>(std::count_if(
            models.begin(), models.end(), [&assetKey](const auto& model) {
                return model.assetKey == assetKey;
            }));
    }

    void ReleaseKeys(std::span<const std::string> keys) {
        for(const auto& key : keys) {
            textures.Release(key);
        }
    }

    void ReleaseUploadKeys() {
        if(uploadJob) {
            ReleaseKeys(uploadJob->acquiredKeys);
            uploadJob->acquiredKeys.clear();
            // A partially uploaded asset that will never be published must not stay resident.
            textures.TrimUnused();
        }
    }

    void AssignAsset(const std::string& assetKey, const std::shared_ptr<GpuMesh>& mesh) {
        for(auto& model : models) {
            if(model.assetKey != assetKey || model.mesh) {
                continue;
            }
            model.mesh = mesh;
            model.localBounds = mesh->bounds;
            model.worldBounds = TransformBounds(mesh->bounds, model.modelMatrix);
            model.error.clear();
        }
        viewCacheDirty = true;
    }

    void PublishAsset(const std::string& assetKey, std::shared_ptr<GpuMesh> mesh) {
        pendingAssets.erase(assetKey);
        if(CountModels(assetKey) == 0) {
            // Every instance was removed while the import was in flight; do not retain residency.
            ReleaseKeys(mesh->textureKeys);
            textures.TrimUnused();
            return;
        }
        meshCache[assetKey] = mesh;
        meshFingerprints[assetKey] = AssetFingerprint(PathFromUtf8(assetKey));
        AssignAsset(assetKey, mesh);
    }

    void PublishFailure(const std::string& assetKey, std::string error) {
        if(!assetKey.empty()) {
            failedAssets[assetKey] = {AssetFingerprint(PathFromUtf8(assetKey)), std::move(error)};
            pendingAssets.erase(assetKey);
        }
        for(auto& model : models) {
            if(model.assetKey == assetKey && !model.mesh) {
                model.error = failedAssets.count(assetKey) != 0 ? failedAssets[assetKey].error
                                                                : std::string("Model import failed");
            }
        }
        viewCacheDirty = true;
    }

    void StartUpload(PendingModelData data, std::string assetKey) {
        UploadJob job;
        job.data = std::move(data);
        job.assetKey = std::move(assetKey);
        job.locations.resize(job.data.textures.size());
        std::vector<bool> referenced(job.data.textures.size(), false);
        for(const auto& submesh : job.data.mesh.submeshes) {
            referenced[submesh.textureIndex] = true;
        }
        for(std::size_t index = 0; index < referenced.size(); ++index) {
            if(referenced[index]) {
                job.textureOrder.push_back(static_cast<std::uint32_t>(index));
            }
        }
        uploadJob = std::move(job);
    }

    UploadOutcome AdvanceUpload(std::size_t& uploadedBytes) {
        auto& job = *uploadJob;
        if(job.nextTexture < job.textureOrder.size()) {
            const auto index = job.textureOrder[job.nextTexture];
            const auto& image = job.data.textures[index];
            try {
                job.locations[index] = textures.Acquire(image);
                job.acquiredKeys.push_back(image.CacheKey());
            } catch(const std::exception& error) {
                const auto key = job.assetKey;
                ReleaseUploadKeys();
                uploadJob.reset();
                PublishFailure(key, error.what());
                return UploadOutcome::Failed;
            }
            uploadedBytes += image.pixels.size();
            ++job.nextTexture;
            return UploadOutcome::InProgress;
        }
        if(!job.buffersUploaded) {
            try {
                job.mesh = std::make_unique<GpuMesh>(context, job.data.mesh, job.locations);
            } catch(const std::exception& error) {
                const auto key = job.assetKey;
                ReleaseUploadKeys();
                uploadJob.reset();
                PublishFailure(key, error.what());
                return UploadOutcome::Failed;
            }
            uploadedBytes += job.data.mesh.vertices.size() * sizeof(MeshVertex) +
                             job.data.mesh.indices.size() * sizeof(std::uint32_t);
            job.buffersUploaded = true;
            return UploadOutcome::InProgress;
        }
        std::shared_ptr<GpuMesh> mesh(job.mesh.release());
        mesh->textureKeys = std::move(job.acquiredKeys);
        const auto key = job.assetKey;
        uploadJob.reset();
        PublishAsset(key, std::move(mesh));
        return UploadOutcome::Finished;
    }

    bool BudgetExhausted(std::chrono::steady_clock::time_point start, std::size_t bytes) const {
        const auto& limits = settings.limits;
        if(limits.maxUploadBytesPerFrame != 0 && bytes >= limits.maxUploadBytesPerFrame) {
            return true;
        }
        if(limits.maxUploadMillisecondsPerFrame > 0.0) {
            const double elapsed =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                    .count();
            if(elapsed >= limits.maxUploadMillisecondsPerFrame) {
                return true;
            }
        }
        return false;
    }

    void MarkDirty() {
        dirty = true;
    }
};

FileParser::FileParser(const GraphicsContext& context, std::filesystem::path sceneFile,
                       const ImportSettings& settings)
    : state(std::make_unique<State>(context, std::move(sceneFile), settings)) {
}

FileParser::~FileParser() {
    Shutdown();
}

bool FileParser::LoadAsset(const std::filesystem::path& path) {
    ModelDescription description;
    description.filePath = PathToUtf8(path);
    return QueueModelLoad(std::move(description));
}

bool FileParser::QueueModelLoad(ModelDescription description) {
    state->context.RequireCurrent();
    if(state->stopped) {
        return false;
    }
    try {
        const auto path =
            std::filesystem::weakly_canonical(std::filesystem::absolute(PathFromUtf8(description.filePath)));
        std::string extension = path.extension().string();
        std::transform(extension.begin(),
                       extension.end(),
                       extension.begin(),
                       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        if(extension != ".obj") {
            Log(LogLevel::Warning, "Unsupported asset type: " + PathToUtf8(path));
            return false;
        }
        description.filePath = PathToUtf8(path);
        const std::string assetKey = description.filePath;
        if(state->nextId == std::numeric_limits<ModelId>::max()) {
            throw std::overflow_error("Model ID capacity exhausted");
        }
        const ModelId id = state->nextId++;
        const auto matrix = MakeModelMatrix(description);
        const std::uint64_t fingerprint = AssetFingerprint(path);

        SceneModel model;
        model.id = id;
        model.description = std::move(description);
        model.modelMatrix = matrix;
        model.assetKey = assetKey;
        model.worldBounds = TransformBounds(model.localBounds, matrix);
        model.mirrored = glm::determinant(glm::mat3(matrix)) < 0.0f;

        if(const auto cached = state->meshCache.find(assetKey); cached != state->meshCache.end()) {
            if(state->meshFingerprints[assetKey] == fingerprint) {
                model.mesh = cached->second;
                model.localBounds = cached->second->bounds;
                model.worldBounds = TransformBounds(cached->second->bounds, matrix);
            }
            else {
                // The source changed on disk; drop the stale asset and re-import below.
                state->ReleaseKeys(cached->second->textureKeys);
                state->meshCache.erase(cached);
                state->meshFingerprints.erase(assetKey);
                state->textures.TrimUnused();
            }
        }
        if(!model.mesh) {
            if(const auto failed = state->failedAssets.find(assetKey);
               failed != state->failedAssets.end()) {
                if(failed->second.fingerprint == fingerprint) {
                    model.error = failed->second.error;
                }
                else {
                    state->failedAssets.erase(failed);
                }
            }
            if(model.error.empty()) {
                if(state->pendingAssets.count(assetKey) != 0) {
                    // Another instance of this asset is already importing; wait for it.
                }
                else if(state->loader.QueueModelLoad(model.description, id)) {
                    state->pendingAssets[assetKey] = id;
                    state->requestPaths[id] = assetKey;
                }
                else {
                    model.error = "Import rejected by the resource budget";
                }
            }
        }
        state->models.push_back(std::move(model));
        state->viewCacheDirty = true;
        state->MarkDirty();
        return true;
    } catch(const std::exception& error) {
        Log(LogLevel::Error, error.what());
    }
    return false;
}

void FileParser::LoadSavedFiles() {
    state->context.RequireCurrent();
    if(state->savedFilesLoaded) {
        return;
    }
    // Do not enable autosave until every persisted entry has been retained successfully.
    auto descriptions = state->config.LoadObjectFromJson();
    for(auto& description : descriptions) {
        const auto path = PathFromUtf8(description.filePath);
        if(path.is_relative()) {
            description.filePath = PathToUtf8(state->sceneFile.parent_path() / path);
        }
        if(!QueueModelLoad(std::move(description))) {
            throw std::runtime_error(
                "Cannot restore the complete scene; the saved configuration is unchanged");
        }
    }
    state->savedFilesLoaded = true;
}

void FileParser::ProcessPendingModels() {
    auto& s = *state;
    s.context.RequireCurrent();
    const auto frameStart = std::chrono::steady_clock::now();
    std::size_t uploadedBytes = 0;
    bool advanced = false;
    std::size_t published = 0;

    for(;;) {
        if(!s.uploadJob) {
            if(s.stopped || published >= s.settings.uploadsPerFrame) {
                break;
            }
            auto completed = s.loader.TakeCompleted(1);
            if(completed.empty()) {
                break;
            }
            auto pending = std::move(completed.front());
            std::string assetKey;
            if(const auto found = s.requestPaths.find(pending.requestId);
               found != s.requestPaths.end()) {
                assetKey = found->second;
                s.requestPaths.erase(found);
            }
            else {
                assetKey = CanonicalAssetKey(pending.description.filePath);
            }
            if(!pending.Success()) {
                s.PublishFailure(assetKey,
                                 pending.error.empty() ? std::string("Model contains no triangles")
                                                       : std::move(pending.error));
                continue;
            }
            s.StartUpload(std::move(pending), std::move(assetKey));
        }
        // Always make at least one step of progress per frame to avoid starving an import.
        if(advanced && s.BudgetExhausted(frameStart, uploadedBytes)) {
            break;
        }
        const auto outcome = s.AdvanceUpload(uploadedBytes);
        advanced = true;
        if(outcome == State::UploadOutcome::Finished) {
            ++published;
        }
    }
    if(advanced) {
        try {
            // Mip generation is batched once per page per frame; a failure must not abort the frame.
            s.textures.FinalizeUploads();
        } catch(const std::exception& error) {
            Log(LogLevel::Error, error.what());
        }
    }
}

const std::vector<ModelView>& FileParser::GetModelViews() const {
    state->context.RequireCurrent();
    if(state->viewCacheDirty) {
        state->viewCache.clear();
        state->viewCache.reserve(state->models.size());
        for(const auto& model : state->models) {
            ModelView view;
            view.id = model.id;
            view.description = model.description;
            view.label = model.description.filePath + "##" + std::to_string(model.id);
            view.loaded = model.mesh != nullptr;
            view.error = model.error;
            state->viewCache.push_back(std::move(view));
        }
        state->viewCacheDirty = false;
    }
    return state->viewCache;
}

std::optional<ModelDescription> FileParser::GetDescription(ModelId id) const {
    state->context.RequireCurrent();
    for(const auto& model : state->models) {
        if(model.id == id) {
            return model.description;
        }
    }
    return std::nullopt;
}

bool FileParser::UpdateTransform(ModelId id, const glm::vec3& position, const glm::vec3& rotation,
                                 const glm::vec3& scale) {
    state->context.RequireCurrent();
    for(auto& model : state->models) {
        if(model.id != id) {
            continue;
        }
        try {
            ModelDescription description = model.description;
            description.position = position;
            description.rotation = rotation;
            description.scale = scale;
            const auto matrix = MakeModelMatrix(description);
            model.description.position = position;
            model.description.rotation = rotation;
            model.description.scale = scale;
            model.modelMatrix = matrix;
            model.worldBounds = TransformBounds(model.localBounds, matrix);
            model.mirrored = glm::determinant(glm::mat3(matrix)) < 0.0f;
            state->viewCacheDirty = true;
            state->MarkDirty();
            return true;
        } catch(const std::exception& error) {
            Log(LogLevel::Warning, error.what());
            return false;
        }
    }
    return false;
}

bool FileParser::UpdateTransform(ModelId id, const ModelDescription& description) {
    return UpdateTransform(id, description.position, description.rotation, description.scale);
}

bool FileParser::RemoveModel(ModelId id) {
    auto& s = *state;
    s.context.RequireCurrent();
    const auto found = std::find_if(s.models.begin(), s.models.end(), [id](const auto& model) {
        return model.id == id;
    });
    if(found == s.models.end()) {
        return false;
    }
    const std::string assetKey = found->assetKey;
    s.models.erase(found);
    if(!assetKey.empty() && s.CountModels(assetKey) == 0) {
        if(s.uploadJob && s.uploadJob->assetKey == assetKey) {
            s.ReleaseUploadKeys();
            s.uploadJob.reset();
        }
        if(const auto pending = s.pendingAssets.find(assetKey); pending != s.pendingAssets.end()) {
            // Stop the queued or in-flight import the user already discarded.
            s.loader.Cancel(pending->second);
            s.requestPaths.erase(pending->second);
            s.pendingAssets.erase(pending);
        }
        if(const auto cached = s.meshCache.find(assetKey); cached != s.meshCache.end()) {
            s.ReleaseKeys(cached->second->textureKeys);
            s.meshCache.erase(cached);
            s.meshFingerprints.erase(assetKey);
        }
        s.textures.TrimUnused();
    }
    s.viewCacheDirty = true;
    s.MarkDirty();
    return true;
}

void FileParser::BuildDrawList(const glm::mat4& viewProjection, const glm::vec3& cameraPosition,
                               std::vector<DrawItem>& out) const {
    state->context.RequireCurrent();
    out.clear();
    const Frustum frustum(viewProjection);
    for(const auto& model : state->models) {
        if(!model.mesh || !frustum.Intersects(model.worldBounds)) {
            continue;
        }
        const auto mvp = viewProjection * model.modelMatrix;
        const glm::vec3 center = (model.worldBounds.minimum + model.worldBounds.maximum) * 0.5f;
        const glm::vec3 delta = center - cameraPosition;
        const float distanceSquared = glm::dot(delta, delta);
        for(const auto& submesh : model.mesh->submeshes) {
            DrawItem item;
            item.vertexArray = &model.mesh->vertexArray;
            item.textureArray = submesh.texture.textureArray;
            item.objectKey = &model;
            item.modelViewProjection = mvp;
            const auto& material = submesh.material;
            item.baseColor = {material.diffuse.r, material.diffuse.g, material.diffuse.b,
                              material.opacity};
            item.uvTransform = {material.textureScale.x, material.textureScale.y,
                                material.textureOffset.x, material.textureOffset.y};
            item.uvWrap = material.clampToEdge ? 1 : 0;
            item.alphaCutoff =
                material.alphaMode == AlphaMode::Mask ? DefaultAlphaCutoff : 0.0f;
            item.blended = material.alphaMode == AlphaMode::Blend;
            item.distanceSquared = distanceSquared;
            item.firstIndex = submesh.range.firstIndex;
            item.indexCount = submesh.range.indexCount;
            item.textureLayer = submesh.texture.layer;
            item.mirrored = model.mirrored;
            out.push_back(item);
        }
    }
    std::sort(out.begin(), out.end(), [](const auto& left, const auto& right) {
        // Opaque and masked geometry first; blended geometry last and back to front.
        if(left.blended != right.blended) {
            return right.blended;
        }
        if(left.blended && left.distanceSquared != right.distanceSquared) {
            return left.distanceSquared > right.distanceSquared;
        }
        // Group submeshes by instance so consecutive draws reuse the object transform.
        if(left.objectKey != right.objectKey) {
            return left.objectKey < right.objectKey;
        }
        if(left.textureArray != right.textureArray) {
            return left.textureArray->GetId() < right.textureArray->GetId();
        }
        if(left.textureLayer != right.textureLayer) {
            return left.textureLayer < right.textureLayer;
        }
        if(left.mirrored != right.mirrored) {
            return left.mirrored < right.mirrored;
        }
        return left.vertexArray->GetId() < right.vertexArray->GetId();
    });
}

std::vector<DrawItem> FileParser::BuildDrawList(const glm::mat4& viewProjection) const {
    std::vector<DrawItem> draws;
    BuildDrawList(viewProjection, glm::vec3(0.0f), draws);
    return draws;
}

bool FileParser::SaveScene() const {
    state->context.RequireCurrent();
    std::vector<ModelDescription> descriptions;
    descriptions.reserve(state->models.size());
    for(const auto& model : state->models) {
        // Preserve queued/failed descriptions too, including edits made before GPU upload.
        descriptions.push_back(model.description);
    }
    if(state->config.SaveObject(descriptions)) {
        state->dirty = false;
        return true;
    }
    return false;
}

bool FileParser::IsDirty() const {
    return state->dirty;
}

void FileParser::Shutdown() noexcept {
    if(state->stopped) {
        return;
    }
    state->context.AssertCurrent();
    state->loader.Stop();
    try {
        // Destructor-time saving is best effort; the explicit pre-shutdown save is the primary path.
        if(state->savedFilesLoaded && state->dirty && !SaveScene()) {
            Log(LogLevel::Error, "Scene could not be saved during shutdown");
        }
    } catch(const std::exception& error) {
        Log(LogLevel::Error, error.what());
    }
    try {
        state->ReleaseUploadKeys();
    } catch(...) {
    }
    state->uploadJob.reset();
    state->models.clear();
    state->meshCache.clear();
    state->meshFingerprints.clear();
    state->failedAssets.clear();
    state->pendingAssets.clear();
    state->requestPaths.clear();
    state->viewCache.clear();
    state->textures.ReleaseAll();
    state->stopped = true;
}
