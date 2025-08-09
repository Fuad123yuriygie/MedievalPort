#pragma once

#include <glad/glad.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

class TextureArray {
public:
    TextureArray(std::vector<std::string>& diffuseTextures);
    ~TextureArray();

    void Bind() const;
private:
    GLuint textureArrayID;
    unsigned char* LoadImage(const std::string& fileName, int& width, int& height, int& channels);
    void FreeImage(unsigned char* data);
};