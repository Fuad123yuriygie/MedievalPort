#pragma once

#include "io/SceneTypes.h"

#include <filesystem>

class ConfigManager {
public:
    explicit ConfigManager(std::filesystem::path filePath);
    bool SaveScene(const SceneDocument& scene) const;
    SceneDocument LoadScene() const;

private:
    std::filesystem::path filePath_;
};
