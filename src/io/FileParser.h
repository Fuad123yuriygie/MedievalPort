#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <glm/glm.hpp>

#include "ConfigManager.h"
#include "IndexBuffer.h"
#include "LoadData.h"
#include "VertexArray.h"
#include "VertexBuffer.h"

// Structure to hold saved transform data for a model
struct SavedTransform {
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
};

class FileParser {
public:
    static FileParser& GetInstance() {
        static FileParser instance;
        return instance;
    }

    bool ExtractFileData(const std::string& filePath);
    void LoadSavedFiles();
    // Function to load a file and parse its contents
    // Note: These return references - caller should hold lock if iterating
    std::vector<ModelData>& GetObjVector() { return objVector; }
    std::vector<const char*>& GetModelNames() { return modelNames; }
    std::mutex& GetObjVectorMutex() { return objVectorMutex; }

    // Thread-safe method to add a completed model from loader thread
    void AddModelFromLoader(const class PendingModelData& pendingData);
    void AddModelFromLoaderWithTransform(const class PendingModelData& pendingData, 
                                        const SavedTransform& transform);
    
    // Process pending GPU resource creation on main thread
    void ProcessPendingModels();

private:
    FileParser();
    ~FileParser();

    bool LoadModelFile(const std::string& filePath);
    bool LoadModelFileFromData(const std::vector<float>& vertices,
                               const std::vector<unsigned int>& indices,
                               const std::vector<std::string>& diffuseTextures,
                               const std::vector<int>& materialIndices,
                               const std::string& filePath);
    
    ConfigManager objectConfig;
    std::vector<ModelData> objVector;
    std::vector<const char*> modelNames;
    std::mutex objVectorMutex;  // Protect access to objVector and modelNames
    
    // Store transforms for models being loaded asynchronously
    std::map<std::string, SavedTransform> savedModelTransforms;
    
    // Pending models waiting for GPU resource creation on main thread
    struct PendingGPUModel {
        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        std::vector<std::string> diffuseTextures;
        std::vector<int> materialIndices;
        std::string filePath;
        SavedTransform transform;
    };
    std::vector<PendingGPUModel> pendingGPUModels;
    std::mutex pendingMutex;
};