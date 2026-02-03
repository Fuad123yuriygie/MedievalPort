#include "FileParser.h"
#include "ModelLoaderThread.h"

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
    for(auto& obj : tempObjVector) {
        if(ExtractFileData(obj.filePath)) {
            std::lock_guard<std::mutex> lock(objVectorMutex);
            objVector.back().position = obj.position;
            objVector.back().rotation = obj.rotation;
            objVector.back().scale = obj.scale;
        }
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
