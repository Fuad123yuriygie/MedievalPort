#pragma once

#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <glm/vec3.hpp> // Include GLM header for vec3

#include "LoadModel.h"

// Alias for JSON library
using json = nlohmann::json;

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    bool SaveObject(const std::vector<ModelData>& models);
    std::vector<ModelData> LoadObject();
};