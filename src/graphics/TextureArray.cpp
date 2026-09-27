#include "graphics/TextureArray.h"

#include "graphics/TextureUpload.h"
#include "io/ImageData.h"

#include <glad/glad.h>
#include <stdexcept>

TextureArray::TextureArray(const GraphicsContext& context, int width, int height, int capacity)
    : object(context, GlObject::Kind::Texture, GL_TEXTURE_2D_ARRAY), width(width), height(height),
      capacity(capacity) {
    GLint maxSize = 0;
    GLint maxLayers = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
    if(width <= 0 || height <= 0 || capacity <= 0 || width > maxSize || height > maxSize ||
       capacity > maxLayers) {
        throw std::invalid_argument("Texture array dimensions exceed the driver limits");
    }
    glTextureStorage3D(object.GetId(), 1, GL_RGBA8, width, height, capacity);
    GLint allocatedWidth = 0;
    glGetTextureLevelParameteriv(object.GetId(), 0, GL_TEXTURE_WIDTH, &allocatedWidth);
    if(allocatedWidth != width) {
        throw std::runtime_error("Texture array allocation failed");
    }
    glTextureParameteri(object.GetId(), GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(object.GetId(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(object.GetId(), GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(object.GetId(), GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void TextureArray::UploadLayer(unsigned layer, const DecodedImage& image) {
    object.RequireCurrent();
    const auto byteCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    if(layer >= static_cast<unsigned>(capacity) || image.width != width || image.height != height ||
       image.pixels.size() != byteCount) {
        throw std::invalid_argument("Invalid RGBA texture data or array slice");
    }
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

void TextureArray::Bind(unsigned unit) const {
    object.RequireCurrent();
    glBindTextureUnit(unit, object.GetId());
}
