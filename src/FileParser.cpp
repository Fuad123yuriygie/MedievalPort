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
        std::vector<float> vertices;
        std::vector<unsigned int> indices;

        // Load the model file
        if(LoadModelFile(filePath, vertices, indices)) {
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
    std::vector<ModelData> tempObjVector = objectConfig.LoadObject();
    for(auto& obj : tempObjVector) {
        if(ExtractFileData(obj.filePath)) {
            objVector.back().position = obj.position;
            objVector.back().rotation = obj.rotation;
            objVector.back().scale = obj.scale;
        }
    }
}

bool FileParser::LoadModelFile(const std::string& filePath, std::vector<float>& vertices,
                               std::vector<unsigned int>& indices) {

    // Load the new .obj file
    if(LoadObject(filePath.c_str(), vertices, indices)) {
        std::cout << "Successfully loaded OBJ file: " << filePath << std::endl;

        // Update the OpenGL buffers with the new model data
        VertexArray* va = new VertexArray(); // Declare and initialize the VertexArray object
        VertexBuffer* vb = new VertexBuffer(vertices.data(), vertices.size() * sizeof(float));
        VertexBufferLayout layout;
        layout.Push<float>(3); // Position (x, y, z)
        layout.Push<float>(3); // Normal (nx, ny, nz)
        layout.Push<float>(2); // Texture coordinates (u, v)
        va->AddBuffer(*vb, layout);

        IndexBuffer* ib = new IndexBuffer(indices.data(), indices.size());
        objVector.push_back({va, ib, vb, filePath}); // Store the new model data in the vector
        modelNames.push_back(objVector.back().filePath.data());
        std::cout << "Model data updated successfully." << std::endl;
    }
    else {
        std::cerr << "Failed to load OBJ file: " << filePath << std::endl;
        return false;
    }

    return true;
}
