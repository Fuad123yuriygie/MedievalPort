#include "graphics/TextureArray.h"

#include "graphics/TextureUpload.h"
#include "io/ImageData.h"

#include <algorithm>
#include <glad/glad.h>
#include <stdexcept>
#include <string>

namespace
{
void ClearGlErrors() {
    while(glGetError() != GL_NO_ERROR) {
    }
}

void ThrowOnGlError(const char* what) {
    const GLenum error = glGetError();
    if(error != GL_NO_ERROR) {
        throw std::runtime_error(std::string(what) + " (GL error " +
                                 std::to_string(static_cast<unsigned>(error)) + ")");
    }
}
} // namespace

TextureArray::TextureArray(const GraphicsContext& context, int width, int height, int capacity,
                           bool srgb)
    : object(context, GlObject::Kind::Texture, GL_TEXTURE_2D_ARRAY), width(width), height(height),
      capacity(capacity), mipLevels(1),
      internalFormat(srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8) {
    GLint maxSize = 0;
    GLint maxLayers = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
    if(width <= 0 || height <= 0 || capacity <= 0 || width > maxSize || height > maxSize ||
       capacity > maxLayers) {
        throw std::invalid_argument("Texture array dimensions exceed the driver limits");
    }
    for(int w = width, h = height; w > 1 || h > 1; w = std::max(1, w / 2), h = std::max(1, h / 2)) {
        ++mipLevels;
    }
    ClearGlErrors();
    // Immutable storage with the full mip chain so filtering never samples an incomplete level.
    glTextureStorage3D(object.GetId(), mipLevels, internalFormat, width, height, capacity);
    ThrowOnGlError("Texture array allocation failed");
    GLint allocatedWidth = 0;
    glGetTextureLevelParameteriv(object.GetId(), 0, GL_TEXTURE_WIDTH, &allocatedWidth);
    if(allocatedWidth != width) {
        throw std::runtime_error("Texture array allocation failed");
    }
    glTextureParameteri(object.GetId(), GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(object.GetId(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(object.GetId(), GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(object.GetId(), GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(object.GetId(), GL_TEXTURE_MAX_LEVEL, mipLevels - 1);
}

void TextureArray::UploadLayer(unsigned layer, const DecodedImage& image) {
    object.RequireCurrent();
    const auto byteCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    if(layer >= static_cast<unsigned>(capacity) || image.width != width || image.height != height ||
       image.pixels.size() != byteCount) {
        throw std::invalid_argument("Invalid RGBA texture data or array slice");
    }
    ClearGlErrors();
    {
        const PixelUnpackScope unpack;
        glTextureSubImage3D(object.GetId(),
                            0,
                            0,
                            0,
                            static_cast<GLint>(layer),
                            width,
                            height,
                            1,
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            image.pixels.data());
    }
    // The transfer is only cached once it has demonstrably succeeded; GL errors do not throw.
    ThrowOnGlError("Texture layer upload failed");
    mipmapsPending = true;
}

void TextureArray::GenerateMipmaps() {
    object.RequireCurrent();
    if(!mipmapsPending) {
        return;
    }
    ClearGlErrors();
    glGenerateTextureMipmap(object.GetId());
    ThrowOnGlError("Texture mipmap generation failed");
    mipmapsPending = false;
}

void TextureArray::Bind(unsigned unit) const {
    object.RequireCurrent();
    glBindTextureUnit(unit, object.GetId());
}

std::size_t TextureArray::GetBytes() const {
    std::size_t bytes = 0;
    for(int level = 0, w = width, h = height; level < mipLevels;
        ++level, w = std::max(1, w / 2), h = std::max(1, h / 2)) {
        bytes += static_cast<std::size_t>(w) * static_cast<std::size_t>(h) *
                 static_cast<std::size_t>(capacity) * 4;
    }
    return bytes;
}
