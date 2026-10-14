#include "io/ConfigManager.h"
#include "io/PathUtils.h"
#include "utils/Log.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace {
void ValidateDescription(const ModelDescription& model) {
    if(model.filePath.empty() || model.filePath.find('\0') != std::string::npos) {
        throw std::invalid_argument("Model filePath must be a nonempty path without NUL characters");
    }
    for(int i = 0; i < 3; ++i) {
        if(!std::isfinite(model.position[i]) || !std::isfinite(model.rotation[i]) ||
           !std::isfinite(model.scale[i])) {
            throw std::invalid_argument("Model transforms must contain finite numbers");
        }
    }
}

float ReadNumber(const nlohmann::json& number, const char* name, double minimum, double maximum) {
    if(!number.is_number()) {
        throw std::invalid_argument(std::string(name) + " must contain numbers");
    }
    const double value = number.get<double>();
    if(!std::isfinite(value) || value < minimum || value > maximum) {
        throw std::invalid_argument(std::string(name) + " contains an out-of-range value");
    }
    return static_cast<float>(value);
}

glm::vec3 ReadTransform(const nlohmann::json& record, const char* name,
                        double minimum = -std::numeric_limits<float>::max(),
                        double maximum = std::numeric_limits<float>::max()) {
    const auto& values = record.at(name);
    if(!values.is_array() || values.size() != 3) {
        throw std::invalid_argument(std::string(name) + " must be an array of three numbers");
    }
    glm::vec3 result;
    for(int i = 0; i < 3; ++i) {
        result[i] = ReadNumber(values.at(i), name, minimum, maximum);
    }
    return result;
}

struct TemporaryConfig {
    std::filesystem::path directory;
    std::filesystem::path file;

    ~TemporaryConfig() {
        try {
            std::error_code ignored;
            if(!file.empty()) {
                std::filesystem::remove(file, ignored);
            }
            if(!directory.empty()) {
                std::filesystem::remove(directory, ignored);
            }
        }
        catch(...) {
        }
    }
};
} // namespace

ConfigManager::ConfigManager(std::filesystem::path filePath) : filePath_(std::move(filePath)) {
    if(filePath_.empty()) {
        throw std::invalid_argument("Configuration path cannot be empty");
    }
}

bool ConfigManager::SaveScene(const SceneDocument& scene) const {
    try {
        const auto lighting = ValidateSceneLighting(scene.lighting);
        nlohmann::json root = {
            {"model", nlohmann::json::array()},
            {"lighting", {{"enabled", lighting.enabled},
                          {"direction", {lighting.direction.x, lighting.direction.y, lighting.direction.z}},
                          {"color", {lighting.color.x, lighting.color.y, lighting.color.z}},
                          {"intensity", lighting.intensity},
                          {"ambientStrength", lighting.ambientStrength}}}};
        for(const auto& model : scene.models) {
            ValidateDescription(model);
            root["model"].push_back({{"filePath", PathToUtf8(PathFromUtf8(model.filePath))},
                                     {"position", {model.position.x, model.position.y, model.position.z}},
                                     {"rotation", {model.rotation.x, model.rotation.y, model.rotation.z}},
                                     {"scale", {model.scale.x, model.scale.y, model.scale.z}}});
        }

        TemporaryConfig temporary;
        static std::atomic<std::uint64_t> sequence{0};
        // An exclusively created sibling directory avoids truncating another writer's temporary file.
        for(int attempt = 0; attempt < 16 && temporary.directory.empty(); ++attempt) {
            auto candidate = filePath_;
            candidate += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                         "-" + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
            std::error_code error;
            if(std::filesystem::create_directory(candidate, error)) {
                temporary.directory = std::move(candidate);
            }
            else if(error && error != std::errc::file_exists) {
                throw std::filesystem::filesystem_error("Cannot create config temporary directory", candidate, error);
            }
        }
        if(temporary.directory.empty()) {
            throw std::runtime_error("Cannot allocate a unique config temporary directory");
        }
        temporary.file = temporary.directory / "scene.json";
        std::ofstream output(temporary.file, std::ios::binary | std::ios::trunc);
        if(!output) {
            throw std::runtime_error("Cannot open config temporary file");
        }
        output << root.dump(4) << '\n';
        output.flush();
        const bool written = output.good();
        output.close();
        if(!written || output.fail()) {
            throw std::runtime_error("Cannot write or close config temporary file");
        }
        // Never delete the original first: a failed replacement must leave it intact.
        std::filesystem::rename(temporary.file, filePath_);
        return true;
    }
    catch(const std::exception& error) {
        Log(LogLevel::Error, std::string("Cannot save configuration: ") + error.what());
        return false;
    }
}

SceneDocument ConfigManager::LoadScene() const {
    try {
        if(!std::filesystem::exists(filePath_)) {
            return {};
        }
        std::ifstream input(filePath_, std::ios::binary);
        if(!input) {
            throw std::runtime_error("Cannot open configuration file");
        }
        const auto root = nlohmann::json::parse(input);
        if(input.bad()) {
            throw std::runtime_error("Cannot read configuration file");
        }
        if(root.is_null()) {
            // The original empty-scene writer emitted JSON null rather than an empty model array.
            return {};
        }
        if(!root.is_object() || !root.contains("model") || !root.at("model").is_array()) {
            throw std::invalid_argument("Configuration must contain a model array");
        }
        SceneDocument scene;
        if(root.contains("lighting")) {
            const auto& lighting = root.at("lighting");
            if(!lighting.is_object() || !lighting.contains("enabled") ||
               !lighting.at("enabled").is_boolean() || !lighting.contains("direction") ||
               !lighting.contains("color") || !lighting.contains("intensity") ||
               !lighting.contains("ambientStrength")) {
                throw std::invalid_argument("Lighting must contain enabled, direction, color, intensity and ambientStrength");
            }
            scene.lighting.enabled = lighting.at("enabled").get<bool>();
            scene.lighting.direction = ReadTransform(lighting, "direction");
            scene.lighting.color = ReadTransform(lighting, "color", 0.0, 1.0);
            scene.lighting.intensity = ReadNumber(lighting.at("intensity"), "intensity", 0.0, 4.0);
            scene.lighting.ambientStrength =
                ReadNumber(lighting.at("ambientStrength"), "ambientStrength", 0.0, 1.0);
            scene.lighting = ValidateSceneLighting(scene.lighting);
        }
        scene.models.reserve(root.at("model").size());
        for(const auto& record : root.at("model")) {
            if(!record.is_object() || !record.contains("filePath") || !record.at("filePath").is_string()) {
                throw std::invalid_argument("Model record must contain a string filePath");
            }
            ModelDescription model;
            model.filePath = record.at("filePath").get<std::string>();
            model.position = ReadTransform(record, "position");
            model.rotation = ReadTransform(record, "rotation");
            model.scale = ReadTransform(record, "scale");
            ValidateDescription(model);
            model.filePath = PathToUtf8(PathFromUtf8(model.filePath));
            scene.models.push_back(std::move(model));
        }
        return scene;
    }
    catch(const std::exception& error) {
        Log(LogLevel::Error, std::string("Cannot load configuration: ") + error.what());
        throw;
    }
}
