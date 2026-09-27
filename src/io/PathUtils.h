#pragma once

#include <filesystem>
#include <string>
#include <string_view>

// GLFW, ImGui, OBJ/MTL text and JSON use UTF-8; filesystem paths use native encoding.
inline std::filesystem::path PathFromUtf8(std::string_view value) {
    auto path = std::filesystem::path(std::u8string(value.begin(), value.end()));
    path.make_preferred();
    return path;
}

inline std::string PathToUtf8(const std::filesystem::path& path) {
    const auto value = path.generic_u8string();
    return std::string(value.begin(), value.end());
}
