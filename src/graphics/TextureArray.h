#pragma once

#include "graphics/GlObject.h"

struct DecodedImage;

class TextureArray {
public:
    explicit TextureArray(const GraphicsContext& context, int width, int height, int capacity);
    ~TextureArray() = default;
    TextureArray(const TextureArray&) = delete;
    TextureArray& operator=(const TextureArray&) = delete;
    TextureArray(TextureArray&&) noexcept = default;
    TextureArray& operator=(TextureArray&&) noexcept = default;

    void UploadLayer(unsigned layer, const DecodedImage& image);
    void Bind(unsigned unit = 0) const;
    unsigned GetId() const {
        return object.GetId();
    }
    int GetCapacity() const {
        return capacity;
    }

private:
    GlObject object;
    int width;
    int height;
    int capacity;
};
