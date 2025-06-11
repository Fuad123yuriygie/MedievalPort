#pragma once

#include <fstream>
#include <glm/vec3.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "LoadModel.h"

using json = nlohmann::json;

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    bool SaveObject(const std::vector<ModelData>& models);
    std::vector<ModelData> LoadObject();
};