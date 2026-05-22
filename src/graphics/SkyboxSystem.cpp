#include "SkyboxSystem.h"

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <ranges>
#include <stb/stb_image.h>
#include <vector>

SkyboxSystem::SkyboxSystem():
    cubemapTexture(0), skyboxVAO(0), skyboxVBO(0), skyboxShader("../res/shaders/SkySphere")
{
    Initialize();
}
SkyboxSystem::~SkyboxSystem() {
}

void SkyboxSystem::Initialize() {
    int cubeWidth, cubeHeight, cubeNrChannels;
    glGenTextures(1, &cubemapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);

    float skyboxVertices[] = {// positions
                            -2.0f,   2.0f, -2.0f, -2.0f, -2.0f, -2.0f,  2.0f, -2.0f, -2.0f,
                              2.0f, -2.0f, -2.0f,  2.0f,  2.0f, -2.0f, -2.0f,  2.0f, -2.0f,

                            -2.0f, -2.0f,  2.0f, -2.0f, -2.0f, -2.0f, -2.0f,  2.0f, -2.0f,
                            -2.0f,  2.0f, -2.0f, -2.0f,  2.0f,  2.0f, -2.0f, -2.0f,  2.0f,

                              2.0f, -2.0f, -2.0f,  2.0f, -2.0f,  2.0f,  2.0f,  2.0f,  2.0f,
                              2.0f,  2.0f,  2.0f,  2.0f,  2.0f, -2.0f,  2.0f, -2.0f, -2.0f,

                              -2.0f, -2.0f, 2.0f,  -2.0f, 2.0f,  2.0f,  2.0f,  2.0f,  2.0f,
                              2.0f,  2.0f,  2.0f,  2.0f,  -2.0f, 2.0f,  -2.0f, -2.0f, 2.0f,

                              -2.0f, 2.0f,  -2.0f, 2.0f,  2.0f,  -2.0f, 2.0f,  2.0f,  2.0f,
                              2.0f,  2.0f,  2.0f,  -2.0f, 2.0f,  2.0f,  -2.0f, 2.0f,  -2.0f,

                              -2.0f, -2.0f, -2.0f, -2.0f, -2.0f, 2.0f,  2.0f,  -2.0f, -2.0f,
                              2.0f,  -2.0f, -2.0f, -2.0f, -2.0f, 2.0f,  2.0f,  -2.0f, 2.0f};
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    std::vector<std::string> faces = {"../res/textures/px.png",  // right
                                      "../res/textures/nx.png",  // left
                                      "../res/textures/py.png",  // up
                                      "../res/textures/ny.png",  // down
                                      "../res/textures/pz.png",  // back
                                      "../res/textures/nz.png"}; // front

    for(unsigned int i = 0; i < faces.size(); i++) {
        unsigned char* data =
            stbi_load(faces[i].c_str(), &cubeWidth, &cubeHeight, &cubeNrChannels, 0);
        cubeNrChannels = (cubeNrChannels == 4) ? GL_RGBA : GL_RGB;
        if(data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0,
                         cubeNrChannels,
                         cubeWidth,
                         cubeHeight,
                         0,
                         cubeNrChannels,
                         GL_UNSIGNED_BYTE,
                         data);
            stbi_image_free(data);
        }
        else {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}
void SkyboxSystem::Update(float deltaTime) {
}
void SkyboxSystem::Render(glm::mat4& view, glm::mat4& projection) {
    glDepthFunc(GL_LEQUAL);
    skyboxShader.Bind();
    glm::mat4 skyboxView = glm::mat4(glm::mat3(view));
    skyboxShader.SetUniformMat4f("u_View", glm::value_ptr(skyboxView));
    skyboxShader.SetUniformMat4f("u_Projection", glm::value_ptr(projection));
    glBindVertexArray(skyboxVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glDepthFunc(GL_LESS);
}

void SkyboxSystem::LoadCubemapTextures() {
}
void SkyboxSystem::SetupSkyboxGeometry() {
}
void SkyboxSystem::BindSkyboxTextures() {
}