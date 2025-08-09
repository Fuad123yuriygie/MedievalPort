#include "FileParser.h"

FileParser::FileParser() {
    // Constructor implementation
}

FileParser::~FileParser() {
    // Save last configuration
    objectConfig.SaveObject(objVector);
    // Cleanup
    for(auto& modelData : objVector) {
        delete modelData.va;
        delete modelData.ib;
        delete modelData.vb;
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
            objVector.back().position = obj.position;
            objVector.back().rotation = obj.rotation;
            objVector.back().scale = obj.scale;
        }
    }
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

        // Update the OpenGL buffers with the new model data
        VertexArray* va = new VertexArray();
        VertexBuffer* vb = new VertexBuffer(vertices.data(), vertices.size() * sizeof(float));

        TextureArray* ta = new TextureArray(diffuseTextures);
        va->AddBuffer(*vb, materialIndices); // Fix this MaterialIndices to be passed correctly

        IndexBuffer* ib = new IndexBuffer(indices.data(), indices.size());
        objVector.push_back({va, ib, vb, ta, filePath}); // Store the new model data in the vector
        modelNames.push_back(objVector.back().filePath.data());
        std::cout << "Model data updated successfully." << std::endl;
    }
    else {
        std::cerr << "Failed to load OBJ file: " << filePath << std::endl;
        return false;
    }

    return true;
}
