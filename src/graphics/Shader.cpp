#include "graphics/Shader.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace
{
struct ShaderStage {
    GLuint id = 0;
    ShaderStage() = default;
    ~ShaderStage() {
        if(id != 0) {
            glDeleteShader(id);
        }
    }
    ShaderStage(const ShaderStage&) = delete;
    ShaderStage& operator=(const ShaderStage&) = delete;
};

std::string ShaderLog(GLuint shader) {
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string message(static_cast<std::size_t>(std::max(length, 1)), '\0');
    GLsizei written = 0;
    glGetShaderInfoLog(shader, static_cast<GLsizei>(message.size()), &written, message.data());
    message.resize(static_cast<std::size_t>(written));
    return message;
}

std::string ProgramLog(GLuint program) {
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::string message(static_cast<std::size_t>(std::max(length, 1)), '\0');
    GLsizei written = 0;
    glGetProgramInfoLog(program, static_cast<GLsizei>(message.size()), &written, message.data());
    message.resize(static_cast<std::size_t>(written));
    return message;
}
} // namespace

Shader::Shader(const GraphicsContext& context, const std::filesystem::path& shaderDirectory)
    : program(context, GlObject::Kind::Program) {
    constexpr std::array<GLenum, 6> types{GL_VERTEX_SHADER,
                                          GL_FRAGMENT_SHADER,
                                          GL_TESS_CONTROL_SHADER,
                                          GL_TESS_EVALUATION_SHADER,
                                          GL_GEOMETRY_SHADER,
                                          GL_COMPUTE_SHADER};
    constexpr std::array<const char*, 6> extensions{".vert",
                                                    ".frag",
                                                    ".tesc",
                                                    ".tese",
                                                    ".geom",
                                                    ".comp"};
    std::array<std::string, types.size()> sources;
    for(std::size_t index = 0; index < types.size(); ++index) {
        const auto path =
            shaderDirectory / (shaderDirectory.filename().string() + extensions[index]);
        if(!std::filesystem::exists(path)) {
            continue;
        }
        std::ifstream file(path);
        if(!file) {
            throw std::runtime_error("Cannot read shader: " + path.string());
        }
        std::ostringstream contents;
        contents << file.rdbuf();
        sources[index] = contents.str();
        if(file.bad() || sources[index].empty()) {
            throw std::runtime_error("Empty or unreadable shader: " + path.string());
        }
    }
    const bool compute = !sources[5].empty();
    const bool anyGraphics = std::any_of(sources.begin(),
                                         sources.begin() + 5,
                                         [](const auto& source) { return !source.empty(); });
    if((compute && anyGraphics) || (!compute && (sources[0].empty() || sources[1].empty())) ||
       (sources[2].empty() != sources[3].empty())) {
        throw std::runtime_error("Missing or incompatible shader stages in " +
                                 shaderDirectory.string());
    }

    std::array<ShaderStage, types.size()> stages;
    for(std::size_t index = 0; index < types.size(); ++index) {
        if(sources[index].empty()) {
            continue;
        }
        auto& stage = stages[index];
        stage.id = glCreateShader(types[index]);
        if(stage.id == 0) {
            throw std::runtime_error("Cannot create shader stage");
        }
        const char* source = sources[index].c_str();
        glShaderSource(stage.id, 1, &source, nullptr);
        glCompileShader(stage.id);
        GLint compiled = GL_FALSE;
        glGetShaderiv(stage.id, GL_COMPILE_STATUS, &compiled);
        if(compiled != GL_TRUE) {
            throw std::runtime_error("Shader compilation failed (" + shaderDirectory.string() +
                                     extensions[index] + "): " + ShaderLog(stage.id));
        }
        glAttachShader(program.GetId(), stage.id);
    }
    glLinkProgram(program.GetId());
    GLint linked = GL_FALSE;
    glGetProgramiv(program.GetId(), GL_LINK_STATUS, &linked);
    if(linked != GL_TRUE) {
        throw std::runtime_error("Shader link failed (" + shaderDirectory.string() +
                                 "): " + ProgramLog(program.GetId()));
    }
    for(const auto& stage : stages) {
        if(stage.id != 0) {
            glDetachShader(program.GetId(), stage.id);
        }
    }
}

void Shader::Bind() const {
    program.RequireCurrent();
    glUseProgram(program.GetId());
}

int Shader::GetUniformLocation(std::string_view name) const {
    program.RequireCurrent();
    if(const auto found = uniformLocations.find(name); found != uniformLocations.end()) {
        return found->second;
    }
    std::string terminatedName(name);
    const int location = glGetUniformLocation(program.GetId(), terminatedName.c_str());
    return uniformLocations.emplace(std::move(terminatedName), location).first->second;
}

void Shader::SetUniformMat4f(int location, const glm::mat4& matrix) const {
    program.RequireCurrent();
    if(location >= 0) {
        glProgramUniformMatrix4fv(program.GetId(), location, 1, GL_FALSE, glm::value_ptr(matrix));
    }
}

void Shader::SetUniform1i(int location, int value) const {
    program.RequireCurrent();
    if(location >= 0) {
        glProgramUniform1i(program.GetId(), location, value);
    }
}

void Shader::SetUniformMat4f(std::string_view name, const glm::mat4& matrix) const {
    SetUniformMat4f(GetUniformLocation(name), matrix);
}

void Shader::SetUniform1i(std::string_view name, int value) const {
    SetUniform1i(GetUniformLocation(name), value);
}
