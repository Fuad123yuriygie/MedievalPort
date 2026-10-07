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
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace {
std::size_t RgbaBytes(int width, int height, std::size_t maxPixels = 0) {
    // stb's dimensions and strides are signed ints, even on 64-bit builds.
    constexpr auto limit = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if(width <= 0 || height <= 0 || static_cast<std::size_t>(width) > limit / 4 ||
       static_cast<std::size_t>(height) > limit / (static_cast<std::size_t>(width) * 4)) {
        throw std::invalid_argument("Invalid or oversized RGBA image dimensions");
    }
    const auto pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if(maxPixels != 0 && pixels > maxPixels) {
        throw std::runtime_error("Image exceeds the configured decoded-pixel budget");
    }
    return pixels * 4;
}

void HashCombine(std::uint64_t& seed, std::uint64_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
}

std::uint64_t SourceFingerprint(const std::filesystem::path& path, std::uintmax_t size,
                                int width, int height, ColorSpace colorSpace) {
    std::uint64_t fingerprint = 0xcbf29ce484222325ull;
    HashCombine(fingerprint, size);
    std::error_code error;
    const auto modified = std::filesystem::last_write_time(path, error);
    if(!error) {
        HashCombine(fingerprint,
                    static_cast<std::uint64_t>(modified.time_since_epoch().count()));
    }
    HashCombine(fingerprint, static_cast<std::uint64_t>(width));
    HashCombine(fingerprint, static_cast<std::uint64_t>(height));
    HashCombine(fingerprint, colorSpace == ColorSpace::Srgb ? 1u : 0u);
    return fingerprint;
}
} // namespace

DecodedImage DecodeImage(const std::filesystem::path& path, bool flipVertical,
                         int targetWidth, int targetHeight, ColorSpace colorSpace,
                         std::size_t maxEncodedBytes, std::size_t maxDecodedPixels) {
    if(targetWidth != 0 || targetHeight != 0) {
        RgbaBytes(targetWidth, targetHeight, maxDecodedPixels);
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
    const auto encodedSize64 = static_cast<std::uintmax_t>(length);
    if(maxEncodedBytes != 0 && encodedSize64 > maxEncodedBytes) {
        throw std::runtime_error("Image exceeds the configured encoded-byte budget: " + pathText);
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
    RgbaBytes(width, height, maxDecodedPixels);
    const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decoded(
        stbi_load_from_memory(encoded.data(), encodedSize, &width, &height, &channels, STBI_rgb_alpha),
        &stbi_image_free);
    if(!decoded) {
        const char* reason = stbi_failure_reason();
        throw std::runtime_error("Cannot decode image: " + pathText + ": " +
                                 (reason ? reason : "unknown decoder error"));
    }
    RgbaBytes(width, height, maxDecodedPixels);

    DecodedImage image;
    image.key = pathText;
    image.colorSpace = colorSpace;
    image.width = targetWidth == 0 ? width : targetWidth;
    image.height = targetHeight == 0 ? height : targetHeight;
    image.contentFingerprint =
        SourceFingerprint(normalizedPath, encodedSize64, image.width, image.height, colorSpace) ^
        (flipVertical ? 0x9e3779b97f4a7c15ull : 0ull);
    image.pixels.resize(RgbaBytes(image.width, image.height, maxDecodedPixels));
    if(image.width == width && image.height == height) {
        std::memcpy(image.pixels.data(), decoded.get(), image.pixels.size());
    }
    else {
        // Resize in the image's own color space: sRGB assets must be filtered in linear light,
        // linear data must stay linear.
        const auto resize = colorSpace == ColorSpace::Srgb
                                ? &stbir_resize_uint8_srgb
                                : &stbir_resize_uint8_linear;
        if(!resize(decoded.get(), width, height, 0, image.pixels.data(), image.width, image.height, 0,
                   STBIR_RGBA)) {
            throw std::runtime_error("Cannot resize image: " + pathText);
        }
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
    image.contentFingerprint = 0xcbf29ce484222325ull;
    HashCombine(image.contentFingerprint, static_cast<std::uint64_t>(size));
    HashCombine(image.contentFingerprint,
                static_cast<std::uint64_t>(color[0]) | (static_cast<std::uint64_t>(color[1]) << 8) |
                    (static_cast<std::uint64_t>(color[2]) << 16) |
                    (static_cast<std::uint64_t>(color[3]) << 24));
    image.pixels.resize(RgbaBytes(size, size));
    for(std::size_t i = 0; i < image.pixels.size(); i += color.size()) {
        std::copy(color.begin(), color.end(), image.pixels.begin() + i);
    }
    return image;
}
