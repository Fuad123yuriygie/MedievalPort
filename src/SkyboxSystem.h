#pragma once

#include <glm/ext/matrix_float4x4.hpp>
#include <ranges>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"

class SkyboxSystem {
public:
    SkyboxSystem();
    ~SkyboxSystem();

    void Update(float deltaTime);
    void Render(glm::mat4& view, glm::mat4& projection);

private:
    unsigned int cubemapTexture;
    unsigned int skyboxVAO;
    unsigned int skyboxVBO;
    Shader skyboxShader;

    void Initialize();
    void LoadCubemapTextures();
    void SetupSkyboxGeometry();
    void BindSkyboxTextures();
};