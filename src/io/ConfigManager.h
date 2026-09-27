#pragma once

#include "io/LoadData.h"

#include <filesystem>
#include <span>
#include <vector>

class ConfigManager {
public:
    explicit ConfigManager(std::filesystem::path filePath);
    bool SaveObject(std::span<const ModelDescription> models) const;
    std::vector<ModelDescription> LoadObjectFromJson() const;

private:
    std::filesystem::path filePath_;
};
