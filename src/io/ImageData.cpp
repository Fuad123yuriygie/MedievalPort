#include "io/ImageData.h"
#include "io/PathUtils.h"

#define STBI_NO_FAILURE_STRINGS
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb/stb_image_resize2.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
std::size_t RgbaBytes(int width, int height) {
    // stb's dimensions and strides are signed ints, even on 64-bit builds.
    constexpr auto limit = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if(width <= 0 || height <= 0 || static_cast<std::size_t>(width) > limit / 4 ||
       static_cast<std::size_t>(height) > limit / (static_cast<std::size_t>(width) * 4)) {
        throw std::invalid_argument("Invalid or oversized RGBA image dimensions");
    }
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
}
} // namespace

DecodedImage DecodeImage(const std::filesystem::path& path, bool flipVertical,
                         int targetWidth, int targetHeight) {
    if(targetWidth != 0 || targetHeight != 0) {
        RgbaBytes(targetWidth, targetHeight);
    }
    const auto normalizedPath = std::filesystem::weakly_canonical(std::filesystem::absolute(path));
    const auto pathText = PathToUtf8(normalizedPath);
    std::ifstream file(normalizedPath, std::ios::binary | std::ios::ate);
    if(!file) {
        throw std::runtime_error("Cannot open image: " + pathText);
    }
    const auto length = file.tellg();
    if(length <= 0 || length > std::numeric_limits<int>::max()) {
        throw std::runtime_error("Invalid or oversized image file: " + pathText);
    }
    std::vector<stbi_uc> encoded(static_cast<std::size_t>(length));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
    if(!file) {
        throw std::runtime_error("Cannot read image: " + pathText);
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    const auto encodedSize = static_cast<int>(encoded.size());
    if(!stbi_info_from_memory(encoded.data(), encodedSize, &width, &height, &channels)) {
        throw std::runtime_error("Invalid image header: " + pathText);
    }
    RgbaBytes(width, height);
    const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decoded(
        stbi_load_from_memory(encoded.data(), encodedSize, &width, &height, &channels, STBI_rgb_alpha),
        &stbi_image_free);
    if(!decoded) {
        const char* reason = stbi_failure_reason();
        throw std::runtime_error("Cannot decode image: " + pathText + ": " +
                                 (reason ? reason : "unknown decoder error"));
    }
    RgbaBytes(width, height);

    DecodedImage image;
    image.key = pathText;
    image.width = targetWidth == 0 ? width : targetWidth;
    image.height = targetHeight == 0 ? height : targetHeight;
    image.pixels.resize(RgbaBytes(image.width, image.height));
    if(image.width == width && image.height == height) {
        std::memcpy(image.pixels.data(), decoded.get(), image.pixels.size());
    }
    else if(!stbir_resize_uint8_linear(decoded.get(), width, height, 0, image.pixels.data(),
                                      image.width, image.height, 0, STBIR_RGBA)) {
        throw std::runtime_error("Cannot resize image: " + pathText);
    }
    if(flipVertical) {
        const auto rowBytes = static_cast<std::size_t>(image.width) * 4;
        for(int y = 0; y < image.height / 2; ++y) {
            auto* top = image.pixels.data() + static_cast<std::size_t>(y) * rowBytes;
            auto* bottom = image.pixels.data() +
                           static_cast<std::size_t>(image.height - 1 - y) * rowBytes;
            std::swap_ranges(top, top + rowBytes, bottom);
        }
    }
    return image;
}

DecodedImage MakeSolidImage(std::string key, int size, std::array<std::uint8_t, 4> color) {
    DecodedImage image;
    image.key = std::move(key);
    image.width = size;
    image.height = size;
    image.pixels.resize(RgbaBytes(size, size));
    for(std::size_t i = 0; i < image.pixels.size(); i += color.size()) {
        std::copy(color.begin(), color.end(), image.pixels.begin() + i);
    }
    return image;
}
