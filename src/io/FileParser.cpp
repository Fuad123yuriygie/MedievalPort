#include "io/FileParser.h"

#include "core/SceneMath.h"
#include "graphics/GraphicsContext.h"
#include "graphics/IndexBuffer.h"
#include "graphics/Renderer.h"
#include "graphics/TextureArray.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"
#include "graphics/VertexBufferLayout.h"
#include "io/ConfigManager.h"
#include "io/PathUtils.h"
#include "ui/ModelLoaderThread.h"
#include "utils/Log.h"

#include <algorithm>
#include <cctype>
#include <glad/glad.h>
#include <glm/matrix.hpp>
#include <limits>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace
{
struct TextureLocation {
    const TextureArray* textureArray = nullptr;
    std::uint32_t layer = 0;
};

struct GpuSubmesh {
    SubmeshData range;
    TextureLocation texture;
};

struct GpuMesh {
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    VertexArray vertexArray;
    std::vector<GpuSubmesh> submeshes;

    GpuMesh(const GraphicsContext& context, const MeshData& mesh,
            std::span<const TextureLocation> textures)
        : vertexBuffer(context, std::as_bytes(std::span(mesh.vertices))),
          indexBuffer(context, mesh.indices), vertexArray(context) {
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
            submeshes.push_back({range, textures[range.textureIndex]});
        }
    }
};

struct SceneModel {
    ModelId id;
    ModelDescription description;
    glm::mat4 modelMatrix;
    Bounds localBounds;
    Bounds worldBounds;
    std::unique_ptr<GpuMesh> mesh;
    std::string error;
};

struct TexturePage {
    TextureArray textureArray;
    std::uint32_t usedLayers = 0;

    TexturePage(const GraphicsContext& context, const ImportSettings& settings)
        : textureArray(context, settings.textureSize, settings.textureSize,
                       settings.textureLayersPerPage) {
    }
};
} // namespace

struct FileParser::State {
    const GraphicsContext& context;
    ImportSettings settings;
    std::filesystem::path sceneFile;
    ConfigManager config;
    std::vector<SceneModel> models;
    std::vector<std::unique_ptr<TexturePage>> texturePages;
    std::unordered_map<std::string, TextureLocation> textureLocations;
    ModelId nextId = 1;
    bool savedFilesLoaded = false;
    bool stopped = false;
    ModelLoaderThread loader;

    State(const GraphicsContext& context, std::filesystem::path sceneFile,
          const ImportSettings& settings)
        : context(context), settings(settings), sceneFile(std::move(sceneFile)),
          config(this->sceneFile), loader(settings) {
        context.RequireCurrent();
        GLint maxLayers = 0;
        GLint maxSize = 0;
        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
        if(settings.textureSize <= 0 || settings.textureSize > maxSize ||
           settings.textureLayersPerPage <= 0 || settings.uploadsPerFrame == 0) {
            throw std::invalid_argument("Invalid texture import or upload budget settings");
        }
        this->settings.textureLayersPerPage = std::min(settings.textureLayersPerPage, maxLayers);
    }

    TextureLocation UploadTexture(const DecodedImage& image) {
        if(const auto found = textureLocations.find(image.key); found != textureLocations.end()) {
            return found->second;
        }
        if(texturePages.empty() || texturePages.back()->usedLayers >=
                                       static_cast<unsigned>(settings.textureLayersPerPage)) {
            texturePages.push_back(std::make_unique<TexturePage>(context, settings));
        }
        auto& page = *texturePages.back();
        const TextureLocation location{&page.textureArray, page.usedLayers};
        page.textureArray.UploadLayer(page.usedLayers, image);
        textureLocations.emplace(image.key, location);
        ++page.usedLayers;
        return location;
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
            std::filesystem::absolute(PathFromUtf8(description.filePath)).lexically_normal();
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
        const auto matrix = MakeModelMatrix(description);
        if(state->nextId == std::numeric_limits<ModelId>::max()) {
            throw std::overflow_error("Model ID capacity exhausted");
        }
        const ModelId id = state->nextId++;
        state->models.push_back({id, description, matrix, {}, {}, nullptr, {}});
        try {
            if(state->loader.QueueModelLoad(std::move(description), id)) {
                return true;
            }
        } catch(...) {
            state->models.pop_back();
            throw;
        }
        state->models.pop_back();
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
    state->context.RequireCurrent();
    // TakeCompleted moves at most the budget under its mutex. No lock survives GPU upload.
    auto completed = state->loader.TakeCompleted(state->settings.uploadsPerFrame);
    for(auto& pending : completed) {
        const auto found =
            std::find_if(state->models.begin(), state->models.end(), [&](const auto& model) {
                return model.id == pending.requestId;
            });
        if(found == state->models.end()) {
            continue; // The editor removed this model while its CPU import was in flight.
        }
        auto& model = *found;
        if(!pending.Success()) {
            model.error =
                pending.error.empty() ? "Model contains no triangles" : std::move(pending.error);
            continue;
        }
        try {
            std::vector<TextureLocation> textures;
            textures.reserve(pending.textures.size());
            for(const auto& image : pending.textures) {
                textures.push_back(state->UploadTexture(image));
            }
            const Bounds worldBounds = TransformBounds(pending.mesh.bounds, model.modelMatrix);
            auto mesh = std::make_unique<GpuMesh>(state->context, pending.mesh, textures);
            model.localBounds = pending.mesh.bounds;
            model.worldBounds = worldBounds;
            model.mesh = std::move(mesh);
            model.error.clear();
        } catch(const std::exception& error) {
            model.error = error.what();
            Log(LogLevel::Error,
                "GPU upload failed for " + model.description.filePath + ": " + error.what());
        }
    }
}

std::vector<ModelView> FileParser::GetModelViews() const {
    state->context.RequireCurrent();
    std::vector<ModelView> views;
    views.reserve(state->models.size());
    for(const auto& model : state->models) {
        views.push_back({model.id, model.description, model.mesh != nullptr, model.error});
    }
    return views;
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

bool FileParser::UpdateTransform(ModelId id, const ModelDescription& description) {
    state->context.RequireCurrent();
    for(auto& model : state->models) {
        if(model.id != id) {
            continue;
        }
        try {
            const auto matrix = MakeModelMatrix(description);
            const auto bounds = TransformBounds(model.localBounds, matrix);
            model.description.position = description.position;
            model.description.rotation = description.rotation;
            model.description.scale = description.scale;
            model.modelMatrix = matrix;
            model.worldBounds = bounds;
            return true;
        } catch(const std::exception& error) {
            Log(LogLevel::Warning, error.what());
            return false;
        }
    }
    return false;
}

bool FileParser::RemoveModel(ModelId id) {
    state->context.RequireCurrent();
    return std::erase_if(state->models, [id](const auto& model) { return model.id == id; }) != 0;
}

std::vector<DrawItem> FileParser::BuildDrawList(const glm::mat4& viewProjection) const {
    state->context.RequireCurrent();
    const Frustum frustum(viewProjection);
    std::vector<DrawItem> draws;
    for(const auto& model : state->models) {
        if(!model.mesh || !frustum.Intersects(model.worldBounds)) {
            continue;
        }
        const auto mvp = viewProjection * model.modelMatrix;
        const bool mirrored = glm::determinant(glm::mat3(model.modelMatrix)) < 0.0f;
        for(const auto& submesh : model.mesh->submeshes) {
            draws.push_back({&model.mesh->vertexArray,
                             submesh.texture.textureArray,
                             mvp,
                             submesh.range.firstIndex,
                             submesh.range.indexCount,
                             submesh.texture.layer,
                             mirrored});
        }
    }
    std::sort(draws.begin(), draws.end(), [](const auto& left, const auto& right) {
        if(left.textureArray != right.textureArray) {
            return left.textureArray->GetId() < right.textureArray->GetId();
        }
        if(left.mirrored != right.mirrored) {
            return left.mirrored < right.mirrored;
        }
        return left.vertexArray->GetId() < right.vertexArray->GetId();
    });
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
    return state->config.SaveObject(descriptions);
}

void FileParser::Shutdown() noexcept {
    if(state->stopped) {
        return;
    }
    state->context.AssertCurrent();
    state->loader.Stop();
    try {
        if(state->savedFilesLoaded && !SaveScene()) {
            Log(LogLevel::Error, "Scene could not be saved during shutdown");
        }
    } catch(const std::exception& error) {
        Log(LogLevel::Error, error.what());
    }
    state->models.clear();
    state->textureLocations.clear();
    state->texturePages.clear();
    state->stopped = true;
}
