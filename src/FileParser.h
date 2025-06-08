#pragma once
#include <string>
#include <vector>

#include "LoadModel.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "ConfigManager.h"

class FileParser
{
public:
    FileParser();
    ~FileParser();

    bool ExtractFileData(const std::string& filePath);
    void LoadSavedFiles();
    // Function to load a file and parse its contents
    std::vector<ModelData> objVector;
    std::vector<const char*> modelNames;
private:
    bool LoadModelFile(const std::string& filePath, std::vector<float>& vertices, std::vector<unsigned int>& indices);
    ConfigManager objectConfig;
};