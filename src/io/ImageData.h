#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Ordinary base-color images are sRGB encoded; normals, masks and other numeric data are linear.
// The tag travels with the pixels so that CPU resizing and GPU storage can treat them correctly.
enum class ColorSpace : std::uint8_t {
    Srgb,
    Linear
};

struct DecodedImage {
    std::string key;
    // Identifies the decoded contents (source fingerprint plus decode parameters). A path alone
    // cannot tell whether the image on disk has changed.
    std::uint64_t contentFingerprint = 0;
    int width = 0;
    int height = 0;
    ColorSpace colorSpace = ColorSpace::Srgb;
    std::vector<std::uint8_t> pixels;

    // Cache identity: same path, same contents, same decode parameters.
    std::string CacheKey() const {
        return key + '#' + std::to_string(contentFingerprint);
    }
};

// Decode once to tightly packed RGBA8, optionally resizing before returning CPU-owned pixels.
// maxEncodedBytes and maxDecodedPixels of zero mean "no explicit limit"; both are enforced before
// the corresponding allocation.
DecodedImage DecodeImage(const std::filesystem::path& path, bool flipVertical, int targetWidth = 0,
                         int targetHeight = 0, ColorSpace colorSpace = ColorSpace::Srgb,
                         std::size_t maxEncodedBytes = 0, std::size_t maxDecodedPixels = 0);
DecodedImage MakeSolidImage(std::string key, int size,
                            std::array<std::uint8_t, 4> color = {255, 255, 255, 255});
