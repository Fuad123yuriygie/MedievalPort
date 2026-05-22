#include "ConfigManager.h"
#include <glm/glm.hpp>

ConfigManager::ConfigManager() {
}

ConfigManager::~ConfigManager() {
}

bool ConfigManager::SaveObject(const std::vector<ModelData>& models) {
    json eachModelConfig, modelElementConfig;
    for(auto& model : models) {
        eachModelConfig["filePath"] = model.filePath;
        eachModelConfig["position"] = {model.position.x, model.position.y, model.position.z};
        eachModelConfig["rotation"] = {model.rotation.x, model.rotation.y, model.rotation.z};
        eachModelConfig["scale"] = {model.scale.x, model.scale.y, model.scale.z};

        modelElementConfig["model"].push_back(eachModelConfig);
    }
    std::ofstream outFile("model_config.json");
    if(outFile.is_open()) {
        outFile << modelElementConfig.dump(4);
        outFile.close();
    }
    else {
        std::cerr << "Failed to save rotation values to file." << std::endl;
    }

    return false;
}

std::vector<ModelData> ConfigManager::LoadObjectFromJson() {
    std::vector<ModelData> models;
    ModelData model;
    json modelElementConfig;
    std::ifstream inFile("model_config.json");
    if(inFile.is_open()) {
        inFile >> modelElementConfig;
        inFile.close();
    }
    else {
        std::cerr << "No rotation configuration file found. Using default values." << std::endl;
    }
    for(auto& eachModelConfig : modelElementConfig["model"]) {
        model.filePath = eachModelConfig["filePath"].template get<std::string>();
        model.position.x = eachModelConfig["position"].at(0);
        model.position.y = eachModelConfig["position"].at(1);
        model.position.z = eachModelConfig["position"].at(2);
        model.rotation.x = eachModelConfig["rotation"].at(0);
        model.rotation.y = eachModelConfig["rotation"].at(1);
        model.rotation.z = eachModelConfig["rotation"].at(2);
        model.scale.x = eachModelConfig["scale"].at(0);
        model.scale.y = eachModelConfig["scale"].at(1);
        model.scale.z = eachModelConfig["scale"].at(2);

        models.push_back(model);
    }
    return models;
}
