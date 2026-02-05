#include "FileParser.h"
#include "ModelLoaderThread.h"
#include <glm/glm.hpp>

FileParser::FileParser() {
    // Constructor implementation
}

FileParser::~FileParser() {
    // Save last configuration
    {
        std::lock_guard<std::mutex> lock(objVectorMutex);
        objectConfig.SaveObject(objVector);
        // Cleanup
        for(auto& modelData : objVector) {
            delete modelData.va;
            delete modelData.ib;
            delete modelData.vb;
        }
    }
}

bool FileParser::ExtractFileData(const std::string& filePath) {
    // Check if the file is an .obj file
    if(filePath.substr(filePath.find_last_of(".") + 1) == "obj") {
        // Load the model file
        if(LoadModelFile(filePath)) {
            return true;
        }
        else {
            std::cerr << "Failed to load model file: " << filePath << std::endl;
            return false;
        }
    }
    else {
        std::cerr << "Unsupported file type: " << filePath << std::endl;
        return false;
    }
    return false;
}

void FileParser::LoadSavedFiles() {
    std::vector<ModelData> tempObjVector = objectConfig.LoadObjectFromJson();
    
    // Queue each model file to load asynchronously
    for(auto& obj : tempObjVector) {
        // Capture the transform by value so duplicate file paths keep distinct transforms
        SavedTransform transform{obj.position, obj.rotation, obj.scale};
        std::string path = obj.filePath;

        // Queue the model to load in background
        ModelLoaderThread::GetInstance().QueueModelLoad(path,
            [this, path, transform](const PendingModelData& pendingData) {
                this->AddModelFromLoaderWithTransform(pendingData, transform);
            }
        );
    }
}

// Helper function to apply saved transforms to loaded models
void FileParser::AddModelFromLoaderWithTransform(const PendingModelData& pendingData, 
                                                  const SavedTransform& transform) {
    if (pendingData.loadSuccess) {
        // Queue for GPU resource creation on main thread, don't create GPU resources here
        std::lock_guard<std::mutex> lock(pendingMutex);
        pendingGPUModels.push_back({
            pendingData.vertices,
            pendingData.indices,
            pendingData.diffuseTextures,
            pendingData.materialIndices,
            pendingData.filePath,
            transform
        });
    }
    else {
        std::cerr << "Failed to load model from loader thread: " << pendingData.filePath << std::endl;
    }
}

// Refactored to handle pre-loaded data (used by loader thread)
bool FileParser::LoadModelFileFromData(const std::vector<float>& vertices,
                                       const std::vector<unsigned int>& indices,
                                       const std::vector<std::string>& diffuseTextures,
                                       const std::vector<int>& materialIndices,
                                       const std::string& filePath) {
    std::cout << "Creating GPU resources for model: " << filePath << std::endl;

    // Create OpenGL resources (must be done on main thread)
    VertexArray* va = new VertexArray();
    VertexBuffer* vb = new VertexBuffer(vertices.data(), vertices.size() * sizeof(float));

    TextureArray* ta = new TextureArray(const_cast<std::vector<std::string>&>(diffuseTextures));
    va->AddBuffer(*vb, const_cast<std::vector<int>&>(materialIndices));

    IndexBuffer* ib = new IndexBuffer(indices.data(), indices.size());
    
    {
        std::lock_guard<std::mutex> lock(objVectorMutex);
        objVector.push_back({va, ib, vb, ta, filePath});
        modelNames.push_back(objVector.back().filePath.data());
    }
    
    std::cout << "Model GPU resources created successfully." << std::endl;
    return true;
}

// Update it so that this can be called independently without initializing an object
bool FileParser::LoadModelFile(const std::string& filePath) {
        
        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        std::vector<std::string> diffuseTextures;
        std::vector<int> materialIndices;
        // Load the new .obj file
        if(LoadObject(filePath.c_str(), vertices, indices, diffuseTextures, materialIndices)) {
        std::cout << "Successfully loaded OBJ file: " << filePath << std::endl;

        return LoadModelFileFromData(vertices, indices, diffuseTextures, materialIndices, filePath);
    }
    else {
        std::cerr << "Failed to load OBJ file: " << filePath << std::endl;
        return false;
    }
}

// Thread-safe method to add a completed model from the loader thread
void FileParser::AddModelFromLoader(const PendingModelData& pendingData) {
    if (pendingData.loadSuccess) {
        LoadModelFileFromData(pendingData.vertices, pendingData.indices, 
                             pendingData.diffuseTextures, pendingData.materialIndices,
                             pendingData.filePath);
    }
    else {
        std::cerr << "Failed to load model from loader thread: " << pendingData.filePath << std::endl;
    }
}
// Process pending models and create GPU resources on main thread
void FileParser::ProcessPendingModels() {
    std::lock_guard<std::mutex> lock(pendingMutex);
    
    for (auto& pending : pendingGPUModels) {
        // Now safe to create GPU resources on main thread
        LoadModelFileFromData(pending.vertices, pending.indices, 
                             pending.diffuseTextures, pending.materialIndices,
                             pending.filePath);
        
        // Apply the saved transform to the loaded model
        {
            std::lock_guard<std::mutex> objLock(objVectorMutex);
            if (!objVector.empty()) {
                objVector.back().position = pending.transform.position;
                objVector.back().rotation = pending.transform.rotation;
                objVector.back().scale = pending.transform.scale;
            }
        }
    }
    
    pendingGPUModels.clear();
}