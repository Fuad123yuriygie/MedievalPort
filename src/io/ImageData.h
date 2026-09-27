#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct DecodedImage {
    std::string key;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

// Decode once to tightly packed RGBA8, optionally resizing before returning CPU-owned pixels.
DecodedImage DecodeImage(const std::filesystem::path& path, bool flipVertical, int targetWidth = 0,
                         int targetHeight = 0);
DecodedImage MakeSolidImage(std::string key, int size,
                            std::array<std::uint8_t, 4> color = {255, 255, 255, 255});
