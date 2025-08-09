#include "TextureArray.h"
#include <stb/stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb/stb_image_resize2.h>

TextureArray::TextureArray(std::vector<std::string>& diffuseTextures) {
    stbi_set_flip_vertically_on_load(true);
    glGenTextures(1, &textureArrayID);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArrayID);

    int texWidth = 0, texHeight = 0, texChannels = 0;
    size_t numTextures = diffuseTextures.size();

    if(numTextures == 0) {
        diffuseTextures.push_back({"../res/textures/monkey.png"}); // Fallback to a default texture
        numTextures = 1;
    }

    // First, find the maximum width and height
    int maxWidth = 512, maxHeight = 512;
    for(const auto& texPath : diffuseTextures) {
        int w, h, c;
        unsigned char* data = LoadImage(texPath, w, h, c);
        if(data) {
            if(w > maxWidth)
                maxWidth = w;
            if(h > maxHeight)
                maxHeight = h;
            FreeImage(data);
        }
        else {
            data = LoadImage("../res/textures/monkey.png", w, h, c);
            if(data) {
                if(w > maxWidth)
                    maxWidth = w;
                if(h > maxHeight)
                    maxHeight = h;
                FreeImage(data);
            }
        }
    }

    // Allocate the texture array with the largest dimensions
    glTexImage3D(GL_TEXTURE_2D_ARRAY,
                 0,
                 GL_RGBA,
                 maxWidth,
                 maxHeight,
                 numTextures,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 nullptr);

    // Now load each texture and upload it, padding if necessary
    for(size_t i = 0; i < numTextures; ++i) {
        int w, h, c;
        unsigned char* data = LoadImage(diffuseTextures[i].c_str(), w, h, c);
        if(data) {
            // If the image is smaller, create a padded buffer
            if(w != maxWidth || h != maxHeight) {
                std::vector<unsigned char> srcRGBA(w * h * 4, 255);
                // Convert to RGBA if needed
                for(int row = 0; row < h; ++row) {
                    for(int col = 0; col < w; ++col) {
                        int srcIdx = (row * w + col) * c;
                        int dstIdx = (row * w + col) * 4;
                        srcRGBA[dstIdx + 0] = data[srcIdx + 0];
                        srcRGBA[dstIdx + 1] = (c > 1) ? data[srcIdx + 1] : 255;
                        srcRGBA[dstIdx + 2] = (c > 2) ? data[srcIdx + 2] : 255;
                        srcRGBA[dstIdx + 3] = (c > 3) ? data[srcIdx + 3] : 255;
                    }
                }
                std::vector<unsigned char> resized(maxWidth * maxHeight * 4, 0);
                // Use stb_image_resize2's new API
                stbir_resize_uint8_linear(srcRGBA.data(),
                                          w,
                                          h,
                                          0,
                                          resized.data(),
                                          maxWidth,
                                          maxHeight,
                                          0,
                                          STBIR_RGBA);
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                                0,
                                0,
                                0,
                                i,
                                maxWidth,
                                maxHeight,
                                1,
                                GL_RGBA,
                                GL_UNSIGNED_BYTE,
                                resized.data());
            }
            else {
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                                0,
                                0,
                                0,
                                i,
                                maxWidth,
                                maxHeight,
                                1,
                                GL_RGBA,
                                GL_UNSIGNED_BYTE,
                                data);
            }
            FreeImage(data);
        }
        else {
            data = LoadImage("../res/textures/monkey.png", w, h, c);
            if(data) {
                // If the image is smaller, create a padded buffer
                if(w != maxWidth || h != maxHeight) {
                    std::vector<unsigned char> resized(maxWidth * maxHeight * 4, 0);
                    // Convert to RGBA if needed (as before)
                    std::vector<unsigned char> srcRGBA(w * h * 4, 255);
                    for(int row = 0; row < h; ++row) {
                        for(int col = 0; col < w; ++col) {
                            int srcIdx = (row * w + col) * c;
                            int dstIdx = (row * w + col) * 4;
                            srcRGBA[dstIdx + 0] = data[srcIdx + 0];
                            srcRGBA[dstIdx + 1] = (c > 1) ? data[srcIdx + 1] : 255;
                            srcRGBA[dstIdx + 2] = (c > 2) ? data[srcIdx + 2] : 255;
                            srcRGBA[dstIdx + 3] = (c > 3) ? data[srcIdx + 3] : 255;
                        }
                    }
                    stbir_resize_uint8_linear(srcRGBA.data(),
                    w,
                    h,
                    0,
                    resized.data(),
                    maxWidth,
                    maxHeight,
                    0,
                    STBIR_RGBA);
                    glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                        0,
                        0,
                        0,
                        i,
                                    maxWidth,
                                    maxHeight,
                                    1,
                                    GL_RGBA,
                                    GL_UNSIGNED_BYTE,
                                    resized.data());
                                }
                                else {
                                    glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                                    0,
                                    0,
                                    0,
                                    i,
                                    maxWidth,
                                    maxHeight,
                                    1,
                                    GL_RGBA,
                                    GL_UNSIGNED_BYTE,
                                    data);
                                }
                                FreeImage(data);
                            }
                            else {
                std::cerr << "Default texture not found!" << std::endl;
            }
        }
    }
    
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_set_flip_vertically_on_load(false);
}

TextureArray::~TextureArray() {
    glDeleteTextures(1, &textureArrayID);
}

void TextureArray::Bind() const {
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArrayID);
}

unsigned char* TextureArray::LoadImage(const std::string& fileName, int& width, int& height, int& channels) {
    
    // Load entire file into memory first
    std::ifstream file(fileName, std::ios::binary | std::ios::ate);
    if(!file.is_open()) {
        return nullptr;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(size);
    file.read((char*)buffer.data(), size);

    // Load from memory instead of file
    unsigned char* data = stbi_load_from_memory(buffer.data(), size, &width, &height, &channels, 4);

    return data;
}

void TextureArray::FreeImage(unsigned char* data) {
    stbi_image_free(data);
}