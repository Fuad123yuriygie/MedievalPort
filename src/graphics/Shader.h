#pragma once

#include "graphics/GlObject.h"

#include <filesystem>
#include <functional>
#include <glm/mat4x4.hpp>
#include <string>
#include <string_view>
#include <unordered_map>

class Shader {
public:
    explicit Shader(const GraphicsContext& context, const std::filesystem::path& shaderDirectory);
    ~Shader() = default;
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) noexcept = default;
    Shader& operator=(Shader&&) noexcept = default;

    void Bind() const;
    unsigned GetId() const {
        return program.GetId();
    }
    int GetUniformLocation(std::string_view name) const;
    void SetUniformMat4f(int location, const glm::mat4& matrix) const;
    void SetUniform1i(int location, int value) const;
    void SetUniformMat4f(std::string_view name, const glm::mat4& matrix) const;
    void SetUniform1i(std::string_view name, int value) const;

private:
    struct UniformHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view value) const noexcept {
            return std::hash<std::string_view>{}(value);
        }
    };

    GlObject program;
    // Uniform discovery is logically const; hot paths retain the resolved integer locations.
    mutable std::unordered_map<std::string, int, UniformHash, std::equal_to<>> uniformLocations;
};
