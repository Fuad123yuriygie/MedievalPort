#include "Shader.h"

Shader::Shader(const std::string& filePath) : m_FilePath(filePath), m_RendererID(0) {
    ShaderProgramSource source = ParseShader(filePath);
    m_RendererID = CreateShader(source);
}

Shader::~Shader() {
    Unbind();
    glDeleteProgram(m_RendererID);
}

ShaderProgramSource Shader::ParseShader(const std::string& basePath) {
    // Map shader stage to file extension
    const std::vector<std::pair<GLenum, std::string>> shaderFiles = {
        {GL_VERTEX_SHADER,          ".vert"},
        {GL_FRAGMENT_SHADER,        ".frag"},
        {GL_TESS_CONTROL_SHADER,    ".tesc"},
        {GL_TESS_EVALUATION_SHADER, ".tese"},
        {GL_GEOMETRY_SHADER,        ".geom"},
        {GL_COMPUTE_SHADER,         ".comp"}};

    ShaderProgramSource source;
    for(const auto& [type, ext] : shaderFiles) {
        std::string shaderFile =
            basePath + "/" + std::filesystem::path(basePath).filename().string() + ext;
        std::ifstream file(shaderFile);
        if(file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            switch(type) {
            case GL_VERTEX_SHADER:
                source.VertexSource = buffer.str();
                break;
            case GL_FRAGMENT_SHADER:
                source.FragmentSource = buffer.str();
                break;
            case GL_TESS_CONTROL_SHADER:
                source.TessControlSource = buffer.str();
                break;
            case GL_TESS_EVALUATION_SHADER:
                source.TessEvalSource = buffer.str();
                break;
            case GL_GEOMETRY_SHADER:
                source.GeometrySource = buffer.str();
                break;
            case GL_COMPUTE_SHADER:
                source.ComputeSource = buffer.str();
                break;
            }
        }
    }
    return source;
}

unsigned int Shader::CompileShader(unsigned int type, const std::string& source) {
    unsigned int id = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if(result == GL_FALSE) {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        char* message = (char*)alloca(length * sizeof(char));
        glGetShaderInfoLog(id, length, &length, message);
        std::cout << "Failed to compile ";
        std::string name;
        switch(type) {
        case GL_VERTEX_SHADER:
            name = "vertex";
            break;
        case GL_FRAGMENT_SHADER:
            name = "fragment";
            break;
        case GL_GEOMETRY_SHADER:
            name = "geometry";
            break;
        case GL_COMPUTE_SHADER:
            name = "compute";
            break;
        case GL_TESS_CONTROL_SHADER:
            name = "tessellation control";
            break;
        case GL_TESS_EVALUATION_SHADER:
            name = "tessellation evaluation";
            break;

        default:
            break;
        }
        std::cout << name;
        std::cout << "shader!" << std::endl;
        std::cout << message << std::endl;
        glDeleteShader(id);
        return 0;
    }

    return id;
}

unsigned int Shader::CreateShader(const ShaderProgramSource& source) {
    unsigned int program = glCreateProgram();
    std::vector<unsigned int> shaders;

    // Helper lambda to compile and attach if source is not empty
    auto tryAttach = [&](GLenum type, const std::string& src) {
        if(!src.empty()) {
            unsigned int id = CompileShader(type, src);
            if(id) {
                glAttachShader(program, id);
                shaders.push_back(id);
            }
        }
    };

    tryAttach(GL_VERTEX_SHADER, source.VertexSource);
    tryAttach(GL_FRAGMENT_SHADER, source.FragmentSource);
    tryAttach(GL_TESS_CONTROL_SHADER, source.TessControlSource);
    tryAttach(GL_TESS_EVALUATION_SHADER, source.TessEvalSource);
    tryAttach(GL_GEOMETRY_SHADER, source.GeometrySource);
    tryAttach(GL_COMPUTE_SHADER, source.ComputeSource);

    glLinkProgram(program);
    glValidateProgram(program);

    for(auto id : shaders)
        glDeleteShader(id);

    return program;
}

void Shader::Bind() const {
    glUseProgram(m_RendererID);
}

void Shader::Unbind() const {
    glUseProgram(0);
}

void Shader::SetUniform4f(const std::string& name, float v0, float v1, float v2, float v3) {
    glUniform4f(GetUniformLocation(name), v0, v1, v2, v3);
}

void Shader::SetUniform3f(const std::string& name, float v0, float v1, float v2) {
    glUniform3f(GetUniformLocation(name), v0, v1, v2);
}

void Shader::SetUniformMat4f(const std::string& name, const float* matrix) {
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, matrix);
}

void Shader::SetUniform1i(const std::string& name, int value) {
    glUniform1i(GetUniformLocation(name), value);
}

int Shader::GetUniformLocation(const std::string& name) {
    // Get the location of the uniform variable in the shader program
    if(m_UniformLocationCache.find(name) != m_UniformLocationCache.end()) {
        return m_UniformLocationCache[name];
    }

    int location = glGetUniformLocation(m_RendererID, name.c_str());
    if(location == -1) {
        std::cout << "Error getting uniform ";
        std::cout << name;
        std::cout << " location!";
        std::cout << std::endl;
    }
    m_UniformLocationCache[name] = location;
    return location;
}