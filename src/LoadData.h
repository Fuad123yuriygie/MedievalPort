#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "IndexBuffer.h"
#include "TextureArray.h"
#include "VertexArray.h"
#include "VertexBuffer.h"

// Obj vector to store the loaded model data
struct ModelData {
    VertexArray* va;
    IndexBuffer* ib;
    VertexBuffer* vb;
    TextureArray* ta;
    std::string filePath;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
};

bool LoadObject(const std::string& filePath, std::vector<float>& vertices,
                std::vector<unsigned int>& indices, std::vector<std::string>& diffuse_textures,
                std::vector<int>& materialIndices);
