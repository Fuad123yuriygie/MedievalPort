#pragma once
#include <string>
#include <vector>

#include "ConfigManager.h"
#include "IndexBuffer.h"
#include "LoadData.h"
#include "VertexArray.h"
#include "VertexBuffer.h"

class FileParser {
public:
    static FileParser& GetInstance() {
        static FileParser instance;
        return instance;
    }

    bool ExtractFileData(const std::string& filePath);
    void LoadSavedFiles();
    // Function to load a file and parse its contents
    std::vector<ModelData>& GetObjVector() { return objVector; }
    std::vector<const char*>& GetModelNames() { return modelNames; }

private:
    FileParser();
    ~FileParser();

    bool LoadModelFile(const std::string& filePath);
    ConfigManager objectConfig;
    std::vector<ModelData> objVector;
    std::vector<const char*> modelNames;
};