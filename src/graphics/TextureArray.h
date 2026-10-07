#pragma once

#include "graphics/GlObject.h"

#include <cstddef>

struct DecodedImage;

class TextureArray {
public:
    // srgb selects the internal format: sRGB for encoded color assets, linear for numeric data.
    explicit TextureArray(const GraphicsContext& context, int width, int height, int capacity,
                          bool srgb = true);
    ~TextureArray() = default;
    TextureArray(const TextureArray&) = delete;
    TextureArray& operator=(const TextureArray&) = delete;
    TextureArray(TextureArray&&) noexcept = default;
    TextureArray& operator=(TextureArray&&) noexcept = default;

    void UploadLayer(unsigned layer, const DecodedImage& image);
    // Generates the mip chain once for every layer uploaded since the previous call. Batching keeps
    // a populated array from being regenerated after each individual layer upload.
    void GenerateMipmaps();
    void Bind(unsigned unit = 0) const;
    unsigned GetId() const {
        return object.GetId();
    }
    int GetCapacity() const {
        return capacity;
    }
    int GetMipLevels() const {
        return mipLevels;
    }
    std::size_t GetBytes() const;

private:
    GlObject object;
    int width;
    int height;
    int capacity;
    int mipLevels;
    unsigned internalFormat;
    bool mipmapsPending = false;
};
