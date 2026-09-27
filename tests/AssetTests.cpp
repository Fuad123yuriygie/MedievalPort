#include "io/ConfigManager.h"
#include "io/ImageData.h"
#include "io/LoadData.h"
#include "io/PathUtils.h"
#include "ui/ModelLoaderThread.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
using namespace std::chrono_literals;

void Check(bool condition, std::string_view message) {
    if(!condition) {
        throw std::runtime_error(std::string(message));
    }
}

template <class Function> void CheckThrows(Function&& function, std::string_view message) {
    try {
        function();
    } catch(const std::exception&) {
        return;
    }
    throw std::runtime_error(std::string(message));
}

struct TemporaryDirectory {
    std::filesystem::path path;

    TemporaryDirectory() {
        const auto base = std::filesystem::temp_directory_path();
        for(int attempt = 0; attempt < 32; ++attempt) {
            const auto candidate =
                base /
                ("medievalport-assets-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                 std::to_string(attempt));
            if(std::filesystem::create_directory(candidate)) {
                path = candidate;
                return;
            }
        }
        throw std::runtime_error("Cannot create test fixture directory");
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

void WriteFile(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    Check(output.is_open(), "Cannot create fixture");
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.close();
    Check(!output.fail(), "Cannot write fixture");
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    Check(input.is_open(), "Cannot read fixture");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

ModelDescription Describe(const std::filesystem::path& path) {
    ModelDescription description;
    description.filePath = PathToUtf8(path);
    return description;
}

bool Equal(const glm::vec3& a, const glm::vec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

void WritePpm(const std::filesystem::path& path) {
    std::string image = "P6\n2 2\n255\n";
    constexpr std::array<unsigned char, 12> pixels =
        {255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255};
    image.append(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    WriteFile(path, image);
}

void WriteUnknownChunkPng(const std::filesystem::path& path, std::string_view chunkType) {
    constexpr std::array<unsigned char, 33> header = {
        137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68,  82, 0,
        0,   0,  1,  0,  0,  0,  1,  8,  6, 0, 0, 0,  31, 21, 196, 137};
    std::string image(reinterpret_cast<const char*>(header.data()), header.size());
    // Header inspection stops at IDAT. Decode then reaches the unknown critical chunk before
    // inflation.
    image.append("\0\0\0\1IDAT\0\0\0\0\0", 13);
    image.append(4, '\0');
    image.append(chunkType);
    image.append(4, '\0');
    WriteFile(path, image);
}

void ValidateModel(const PendingModelData& model, int textureSize) {
    Check(model.Success(), model.error);
    Check(!model.mesh.vertices.empty(), "Successful model has no vertices");
    Check(!model.textures.empty(), "Successful model has no default texture");
    Check(model.mesh.indices.size() % 3 == 0, "Index buffer is not triangular");
    for(const auto index : model.mesh.indices) {
        Check(index < model.mesh.vertices.size(), "Index exceeds vertex buffer");
    }
    for(const auto& vertex : model.mesh.vertices) {
        for(int axis = 0; axis < 3; ++axis) {
            Check(std::isfinite(vertex.position[axis]) && std::isfinite(vertex.normal[axis]),
                  "Nonfinite mesh attribute");
            Check(vertex.position[axis] >= model.mesh.bounds.minimum[axis] &&
                      vertex.position[axis] <= model.mesh.bounds.maximum[axis],
                  "Incorrect mesh bounds");
        }
        Check(std::isfinite(vertex.texCoord.x) && std::isfinite(vertex.texCoord.y), "Nonfinite UV");
    }
    std::size_t covered = 0;
    for(const auto& submesh : model.mesh.submeshes) {
        Check(submesh.firstIndex == covered, "Submeshes do not cover contiguous index ranges");
        Check(submesh.indexCount != 0 && submesh.indexCount % 3 == 0, "Invalid submesh count");
        Check(submesh.textureIndex < model.textures.size(), "Invalid submesh texture index");
        covered += submesh.indexCount;
    }
    Check(covered == model.mesh.indices.size(), "Submeshes do not cover all indices");
    for(const auto& image : model.textures) {
        Check(image.width == textureSize && image.height == textureSize,
              "Unexpected decoded texture dimensions");
        Check(image.pixels.size() == static_cast<std::size_t>(textureSize) * textureSize * 4,
              "Image is not tightly packed RGBA");
    }
    Check(std::all_of(model.textures[0].pixels.begin(),
                      model.textures[0].pixels.end(),
                      [](std::uint8_t value) { return value == 255; }),
          "Default texture is not white");
}

constexpr std::string_view Triangle = "v -2 0 3\nv 4 0 3\nv -2 5 3\nf 1 2 3\n";
constexpr std::string_view QuadPositions = "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n";

void TestImages(const std::filesystem::path& root) {
    const auto path = root / "images" / std::filesystem::path(u8"tiny-\u00e9-\u7eb9\u7406.ppm");
    WritePpm(path);
    const auto original = DecodeImage(path, false);
    Check(original.width == 2 && original.height == 2 && original.pixels.size() == 16,
          "Decode dimensions");
    Check(original.key == PathToUtf8(std::filesystem::weakly_canonical(path)),
          "Noncanonical UTF-8 image key");
    constexpr std::array<std::uint8_t, 16> expected =
        {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    Check(std::equal(original.pixels.begin(), original.pixels.end(), expected.begin()),
          "Incorrect RGBA conversion");
    const auto flipped = DecodeImage(path, true);
    Check(
        std::equal(flipped.pixels.begin(), flipped.pixels.begin() + 8, original.pixels.begin() + 8),
        "Vertical flip did not exchange rows");
    Check(std::equal(flipped.pixels.begin() + 8, flipped.pixels.end(), original.pixels.begin()),
          "Vertical flip lost a row");
    const auto resized = DecodeImage(path.parent_path() / "." / path.filename(), false, 4, 3);
    Check(resized.width == 4 && resized.height == 3 && resized.pixels.size() == 48,
          "Resize dimensions");
    Check(resized.key == original.key, "Aliases do not share normalized image keys");
    for(std::size_t i = 3; i < resized.pixels.size(); i += 4) {
        Check(resized.pixels[i] == 255, "Resize corrupted alpha");
    }
    const auto solid = MakeSolidImage("test-solid", 3, {17, 29, 43, 127});
    Check(solid.width == 3 && solid.height == 3 && solid.pixels.size() == 36,
          "Solid image dimensions");
    for(std::size_t i = 0; i < solid.pixels.size(); i += 4) {
        Check(solid.pixels[i] == 17 && solid.pixels[i + 1] == 29 && solid.pixels[i + 2] == 43 &&
                  solid.pixels[i + 3] == 127,
              "Solid image color");
    }
    WriteFile(root / "images" / "corrupt.ppm", "not an image");
    WriteFile(root / "images" / "truncated.ppm", "P6\n2 2\n255\nx");
    WriteFile(root / "images" / "oversized.ppm", "P6\n2147483647 2147483647\n255\n");
    CheckThrows([&] { DecodeImage(root / "images" / "corrupt.ppm", false); },
                "Corrupt image accepted");
    CheckThrows([&] { DecodeImage(root / "images" / "truncated.ppm", false); },
                "Truncated image accepted");
    CheckThrows([&] { DecodeImage(root / "images" / "missing.ppm", false); },
                "Missing image accepted");
    CheckThrows([&] { DecodeImage(root / "images" / "oversized.ppm", false); },
                "Oversized image accepted");
    CheckThrows([&] { DecodeImage(path, false, 0, 4); }, "Partial resize dimensions accepted");
    CheckThrows([&] { DecodeImage(path, false, -1, 4); }, "Negative resize dimensions accepted");
    CheckThrows([&] { DecodeImage(path, false, std::numeric_limits<int>::max(), 4); },
                "Overflowing resize accepted");
    CheckThrows([] { MakeSolidImage("bad", 0); }, "Zero-size solid image accepted");
    CheckThrows([] { MakeSolidImage("bad", -1); }, "Negative solid image accepted");
    CheckThrows([] { MakeSolidImage("bad", std::numeric_limits<int>::max()); },
                "Overflowing solid image accepted");

    auto readUnflipped = std::async(std::launch::async, [&] {
        for(int i = 0; i < 12; ++i) {
            Check(DecodeImage(path, false).pixels == original.pixels,
                  "Concurrent unflipped decode changed");
        }
    });
    auto readFlipped = std::async(std::launch::async, [&] {
        for(int i = 0; i < 12; ++i) {
            Check(DecodeImage(path, true).pixels == flipped.pixels,
                  "Concurrent flipped decode changed");
        }
    });
    readUnflipped.get();
    readFlipped.get();

    const auto firstCorrupt = root / "images" / "unknown-first.png";
    const auto secondCorrupt = root / "images" / "unknown-second.png";
    WriteUnknownChunkPng(firstCorrupt, "ABCD");
    WriteUnknownChunkPng(secondCorrupt, "WXYZ");
    auto firstDecoder = std::async(std::launch::async, [&] {
        for(int i = 0; i < 128; ++i) {
            CheckThrows([&] { DecodeImage(firstCorrupt, false); },
                        "Unknown critical PNG chunk accepted");
        }
    });
    auto secondDecoder = std::async(std::launch::async, [&] {
        for(int i = 0; i < 128; ++i) {
            CheckThrows([&] { DecodeImage(secondCorrupt, true); },
                        "Concurrent corrupt PNG accepted");
        }
    });
    firstDecoder.get();
    secondDecoder.get();
}

void TestImporter(const std::filesystem::path& root) {
    ImportSettings settings;
    settings.textureSize = 4;
    const auto plain = root / "plain.OBJ";
    WriteFile(plain, Triangle);
    auto description = Describe(plain);
    description.position = {1.0f, 2.0f, 3.0f};
    description.scale = {-1.0f, 0.0f, 2.0f};
    const auto model = LoadModelData(description, settings);
    ValidateModel(model, 4);
    Check(model.requestId == 0, "Importer assigned a worker request ID");
    Check(model.description.filePath == description.filePath &&
              Equal(model.description.position, description.position) &&
              Equal(model.description.scale, description.scale),
          "Importer lost description transforms");
    Check(model.mesh.vertices.size() == 3 && model.mesh.indices.size() == 3,
          "Plain triangle counts");
    Check(model.textures.size() == 1 && model.mesh.submeshes[0].textureIndex == 0,
          "Missing default material");
    Check(Equal(model.mesh.bounds.minimum, {-2.0f, 0.0f, 3.0f}) &&
              Equal(model.mesh.bounds.maximum, {4.0f, 5.0f, 3.0f}),
          "Incorrect triangle bounds");
    for(const auto& vertex : model.mesh.vertices) {
        Check(Equal(vertex.normal, {0.0f, 0.0f, 1.0f}), "Flat normal was not generated");
        Check(vertex.texCoord.x == 0.0f && vertex.texCoord.y == 0.0f, "Missing UV is not zero");
    }

    const auto quad = root / "quad.obj";
    WriteFile(quad,
              std::string(QuadPositions) +
                  "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvn 0 0 2\nf 1/1/1 2/2/1 3/3/1 4/4/1\n");
    const auto indexed = LoadModelData(Describe(quad), settings);
    ValidateModel(indexed, 4);
    Check(indexed.mesh.vertices.size() == 4 && indexed.mesh.indices.size() == 6,
          "Quad was not triangulated and deduplicated");
    Check(indexed.mesh.indices == std::vector<std::uint32_t>({0, 1, 2, 0, 2, 3}),
          "Quad has no real index reuse");
    WriteFile(quad, std::string(QuadPositions) + "f 1 2 3 4\n");
    const auto flatQuad = LoadModelData(Describe(quad), settings);
    ValidateModel(flatQuad, 4);
    Check(flatQuad.mesh.vertices.size() == 4, "Triangulation split a single flat face's vertices");

    const auto mixed = root / "mixed.obj";
    WriteFile(mixed,
              std::string(QuadPositions) + "vt .25 .75\nvt .5 .5\nvn 0 0 2\nf 1/1/1 2//1 3/2 4\n");
    const auto mixedModel = LoadModelData(Describe(mixed), settings);
    ValidateModel(mixedModel, 4);
    Check(mixedModel.mesh.vertices.size() == 4, "Mixed attribute quad vertex count");
    Check(mixedModel.mesh.vertices[0].texCoord.x == 0.25f &&
              mixedModel.mesh.vertices[0].texCoord.y == 0.75f,
          "Authored UV lost");
    Check(mixedModel.mesh.vertices[1].texCoord.x == 0.0f &&
              mixedModel.mesh.vertices[1].texCoord.y == 0.0f,
          "Missing per-corner UV indexed a nonempty array");
    Check(Equal(mixedModel.mesh.vertices[2].normal, {0.0f, 0.0f, 1.0f}) &&
              Equal(mixedModel.mesh.vertices[3].normal, {0.0f, 0.0f, 1.0f}),
          "Missing per-corner normal failed");
    WriteFile(mixed, "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\nf 1 2 3\nf 1 4 2\n");
    const auto sharp = LoadModelData(Describe(mixed), settings);
    ValidateModel(sharp, 4);
    Check(sharp.mesh.vertices.size() == 6, "Flat normals were merged across different faces");
    Check(Equal(sharp.mesh.vertices[0].normal, {0.0f, 0.0f, 1.0f}) &&
              Equal(sharp.mesh.vertices[3].normal, {0.0f, 1.0f, 0.0f}),
          "Flat normals have incorrect winding");
    WriteFile(mixed,
              "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 1\nvn 0 0 1\nvn 0 0 1\n"
              "f 1/1/1 2/1/1 3/1/1\nf 1/2/2 2/1/1 3/1/1\n");
    const auto seams = LoadModelData(Describe(mixed), settings);
    ValidateModel(seams, 4);
    Check(seams.mesh.vertices.size() == 4, "Deduplication did not preserve normal/UV index seams");
    WriteFile(mixed,
              "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt .5 .5\nvn 0 0 1\nf -3/-1/-1 -2/-1/-1 -1/-1/-1\n");
    ValidateModel(LoadModelData(Describe(mixed), settings), 4);
    WriteFile(mixed, "v 0 0 0\nv 2 0 0\nv 2 2 0\nv 1 1 0\nv 0 2 0\nf 1 2 3 4 5\n");
    const auto concave = LoadModelData(Describe(mixed), settings);
    ValidateModel(concave, 4);
    Check(concave.mesh.vertices.size() == 5 && concave.mesh.indices.size() == 9,
          "Concave polygon triangulation");

    const auto textured = root / "nested" / "model.obj";
    const auto imagePath = root / "nested" / "materials" / "textures" / "tiny.ppm";
    WritePpm(imagePath);
    WriteFile(root / "nested" / "materials" / "textures" / "bad.ppm", "corrupt image");
    WriteFile(root / "nested" / "materials" / "scene.mtl",
              "newmtl first\nmap_Kd textures/tiny.ppm\n"
              "newmtl second\nmap_Kd textures/./tiny.ppm\n"
              "newmtl absent\nmap_Kd textures/missing.ppm\n"
              "newmtl corrupt\nmap_Kd textures/bad.ppm\n"
              "newmtl untextured\nKd 1 1 1\n");
    WriteFile(textured,
              "mtllib materials/scene.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\n"
              "usemtl first\nf 1//1 2//1 3//1\nusemtl second\nf 1//1 2//1 3//1\n"
              "usemtl first\nf 1//1 2//1 3//1\nusemtl absent\nf 1//1 2//1 3//1\n"
              "usemtl corrupt\nf 1//1 2//1 3//1\nusemtl untextured\nf 1//1 2//1 3//1\n");
    const auto texturedModel = LoadModelData(Describe(textured), settings);
    ValidateModel(texturedModel, 4);
    Check(texturedModel.textures.size() == 2,
          "Shared image decoded into duplicate texture entries");
    Check(texturedModel.textures[1].key == PathToUtf8(std::filesystem::weakly_canonical(imagePath)),
          "Nested MTL-relative texture path was not preserved");
    Check(texturedModel.mesh.vertices.size() == 15, "Material index missing from full vertex key");
    Check(texturedModel.mesh.submeshes.size() == 5, "Faces were not grouped by material");
    Check(texturedModel.mesh.submeshes[0].indexCount == 6 &&
              texturedModel.mesh.submeshes[0].textureIndex == 1 &&
              texturedModel.mesh.submeshes[1].textureIndex == 1,
          "Texture sharing or material grouping failed");
    for(std::size_t i = 2; i < texturedModel.mesh.submeshes.size(); ++i) {
        Check(texturedModel.mesh.submeshes[i].textureIndex == 0,
              "Bad/absent material did not use default slice");
    }
    WriteFile(textured, "mtllib missing.mtl\nusemtl missing\n" + std::string(Triangle));
    ValidateModel(LoadModelData(Describe(textured), settings), 4);

    const auto libraries = root / "libraries";
    const auto firstLibraryTexture = libraries / "materials" / "tex" / "first.ppm";
    const auto secondLibraryTexture = libraries / "other materials" / "tex" / "second texture.ppm";
    WritePpm(firstLibraryTexture);
    WritePpm(secondLibraryTexture);
    WriteFile(libraries / "materials" / "first.mtl", "newmtl first\nmap_Kd tex\\first.ppm\n");
    WriteFile(libraries / "other materials" / "second library.mtl",
              "newmtl second\nmap_Kd tex\\second\\ texture.ppm\n");
    const auto multiLibraryModel = libraries / "multi.obj";
    WriteFile(multiLibraryModel,
              "mtllib materials\\first.mtl other\\ materials\\second\\ library.mtl\n"
              "o object_one\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\n"
              "g first_group\nusemtl first\nf 1//1 2//1 3//1\n"
              "o object_two\ng second_group\nusemtl second\nf 1//1 2//1 3//1\n");
    const auto multiLibrary = LoadModelData(Describe(multiLibraryModel), settings);
    ValidateModel(multiLibrary, 4);
    Check(multiLibrary.textures.size() == 3 && multiLibrary.mesh.submeshes.size() == 2 &&
              multiLibrary.mesh.vertices.size() == 6,
          "Later material libraries or shape records were lost");
    Check(multiLibrary.mesh.submeshes[0].textureIndex == 1 &&
              multiLibrary.mesh.submeshes[1].textureIndex == 2,
          "Later library material became a default material");
    Check(multiLibrary.textures[1].key ==
                  PathToUtf8(std::filesystem::weakly_canonical(firstLibraryTexture)) &&
              multiLibrary.textures[2].key ==
                  PathToUtf8(std::filesystem::weakly_canonical(secondLibraryTexture)),
          "Backslash directory separators or escaped spaces were not preserved");

    const auto checkedMaterial = libraries / "materials" / "checked.mtl";
    const auto checkedModel = libraries / "checked.obj";
    WriteFile(checkedModel,
              "mtllib materials/checked.mtl\nusemtl checked\n" + std::string(Triangle));
    WriteFile(checkedMaterial,
              "newmtl checked\nillum 999999999999999999\nKd 1e99999999999 0 0\n"
              "map_Kd -s 1 1 1 -o 0 0 0 -t 0 0 0 -mm 0 1 -boost 1 -bm 1 "
              "-texres 4 -clamp off -blendu on -blendv off -type sphere -imfchan r "
              "-colorspace sRGB tex/first.ppm\r\n");
    const auto checked = LoadModelData(Describe(checkedModel), settings);
    ValidateModel(checked, 4);
    Check(checked.textures.size() == 2 && checked.mesh.submeshes[0].textureIndex == 1,
          "Valid diffuse options or filtering unused material fields lost the texture");
    for(const std::string_view option : {"-texres 2147483648",
                                         "-boost 1e999999999999",
                                         "-o 0 0 1e999999999999",
                                         "-s 1 texture.png 1e999999999999",
                                         "-mm 0 1e999999999999",
                                         "-texres 999999999999999999999999"}) {
        WriteFile(checkedMaterial,
                  "newmtl checked\nmap_Kd " + std::string(option) + " tex/first.ppm\n");
        const auto fallback = LoadModelData(Describe(checkedModel), settings);
        ValidateModel(fallback, 4);
        Check(fallback.textures.size() == 1 && fallback.mesh.submeshes[0].textureIndex == 0,
              "An unrepresentable material option was not dropped before the vendor parser");
    }
    // Non-numeric option arguments are string or on/off values, so the vendor parser cannot
    // overflow on them.
    for(const std::string_view option :
        {"-blendu off -clamp on -type sphere -imfchan r -colorspace sRGB",
         "-s 1 1 1 -mm 0 1 -boost 1 -bm 1 -texres 4"}) {
        WriteFile(checkedMaterial,
                  "newmtl checked\nmap_Kd " + std::string(option) + " tex/first.ppm\n");
        ValidateModel(LoadModelData(Describe(checkedModel), settings), 4);
    }

    const auto unicodeRoot = root / std::filesystem::path(u8"caf\u00e9-\u573a\u666f");
    const auto unicodeModel = unicodeRoot / std::filesystem::path(u8"caf\u00e9-\u6a21\u578b.OBJ");
    const auto unicodeLibrary = std::filesystem::path(u8"mat\u00e9riaux/\u6750\u8d28.mtl");
    const auto unicodeTexture = std::filesystem::path(u8"t\u00e9xtures/\u7eb9\u7406.ppm");
    const auto unicodeTexturePath = unicodeRoot / unicodeLibrary.parent_path() / unicodeTexture;
    WritePpm(unicodeTexturePath);
    WriteFile(unicodeRoot / unicodeLibrary,
              "newmtl unicode\nmap_Kd " + PathToUtf8(unicodeTexture) + "\n");
    WriteFile(unicodeModel,
              "mtllib " + PathToUtf8(unicodeLibrary) + "\nusemtl unicode\n" +
                  std::string(Triangle));
    const auto unicodeImported = LoadModelData(Describe(unicodeModel), settings);
    ValidateModel(unicodeImported, 4);
    Check(unicodeImported.description.filePath == PathToUtf8(unicodeModel),
          "Model description is not UTF-8");
    Check(unicodeImported.textures.size() == 2 &&
              unicodeImported.mesh.submeshes[0].textureIndex == 1,
          "Non-ASCII OBJ, MTL or texture path failed");
    Check(unicodeImported.textures[1].key ==
              PathToUtf8(std::filesystem::weakly_canonical(unicodeTexturePath)),
          "Non-ASCII texture key is not canonical UTF-8");
    Check(PathFromUtf8(unicodeImported.description.filePath) == unicodeModel,
          "UTF-8 model path did not round-trip");

    const std::vector<std::string> unusedDirectives = {"vw 0 2147483648 1\n",
                                                       "vw +-0 +-2147483648 +-1\n",
                                                       "t crease 1/0/0 2147483648\n",
                                                       "t crease 999999999999999999/0/0 1\n",
                                                       "t crease +-2/0/0 +-1\n",
                                                       "s 2147483648\n",
                                                       "s 999999999999999999\n",
                                                       "s +-1\n",
                                                       "g retained\rvw 0 2147483648 1\n"};
    const auto ignored = root / "unused-directives.obj";
    for(const auto& directive : unusedDirectives) {
        WriteFile(ignored,
                  "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 -2\n" + directive + "f 1//1 2//1 3//1\n" +
                      directive);
        const auto filtered = LoadModelData(Describe(ignored), settings);
        ValidateModel(filtered, 4);
        Check(filtered.mesh.vertices.size() == 3 && filtered.mesh.indices.size() == 3,
              "Filtering unused numeric directives changed geometry");
        Check(Equal(filtered.mesh.vertices[0].normal, {0.0f, 0.0f, -1.0f}),
              "Filtering unused numeric directives changed explicit normals");
    }

    const std::vector<std::string> malformed = {
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 99\n",
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 2 3\n",
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nf -99 2 3\n",
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//2 2//1 3//1\n",
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/2 2/1 3/1\n",
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/0 2/1 3/1\n",
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 99999999999999999999 2 3\n",
        "v 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n",
        "v nan 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n",
        "v +-1 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n",
        "v 1e9999 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n",
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//+-1 2//1 3//1\n",
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2\n",
        std::string(QuadPositions) + "f 1 2 3\nf 1 2 3 99\n",
        std::string(QuadPositions) + "f 1 2 3 4 99\n",
        "v 0 0 0\nv 1 0 0\nv 2 0 0\nf 1 2 3\n",
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 0\nf 1//1 2//1 3//1\n",
        ""};
    const auto invalid = root / "invalid.obj";
    for(std::size_t i = 0; i < malformed.size(); ++i) {
        WriteFile(invalid, malformed[i]);
        const auto failed = LoadModelData(Describe(invalid), settings);
        Check(!failed.Success() && !failed.error.empty(),
              "Malformed OBJ accepted: fixture " + std::to_string(i));
        Check(failed.mesh.indices.empty() && failed.textures.empty(),
              "Failed import retained partial data");
        Check(failed.description.filePath == PathToUtf8(invalid), "Failed import lost description");
    }
    Check(!LoadModelData(Describe(root / "missing.obj"), settings).Success(),
          "Missing mesh accepted");
    WriteFile(root / "wrong.obj.txt", Triangle);
    Check(!LoadModelData(Describe(root / "wrong.obj.txt"), settings).Success(),
          "Non-OBJ extension accepted");
    settings.textureSize = 0;
    Check(!LoadModelData(Describe(plain), settings).Success(),
          "Invalid fallback texture size accepted");
    description.position.x = std::numeric_limits<float>::infinity();
    Check(!LoadModelData(description).Success(), "Nonfinite transform accepted by importer");
}

void TestConfig(const std::filesystem::path& root) {
    const auto path = root / "scene.json";
    const ConfigManager config(path);
    Check(config.LoadObjectFromJson().empty(), "Missing config is not an empty scene");
    WriteFile(path, "null\n");
    Check(config.LoadObjectFromJson().empty(), "Legacy null empty scene was rejected");
    Check(ReadFile(path) == "null\n", "Loading a legacy empty scene modified it");
    const std::vector<ModelDescription> empty;
    Check(config.SaveObject(empty), "Cannot save empty scene");
    Check(ReadFile(path).find("\"model\": []") != std::string::npos, "Empty scene schema changed");
    Check(config.LoadObjectFromJson().empty(), "Empty config round-trip failed");
    auto first = Describe(root / "not-yet-loaded.obj");
    first.position = {1.25f, -2.0f, 3.0f};
    first.rotation = {0.5f, 1.0f, -4.0f};
    first.scale = {-2.0f, 0.0f, 0.125f};
    auto second = first;
    second.position = {8.0f, 9.0f, 10.0f};
    second.rotation = {-1.0f, -2.0f, -3.0f};
    second.scale = {1.0f, 2.0f, 3.0f};
    std::vector<ModelDescription> descriptions{first, second};
    Check(config.SaveObject(descriptions), "Cannot atomically replace existing config");
    const auto loaded = config.LoadObjectFromJson();
    Check(loaded.size() == descriptions.size(), "Duplicate paths were merged");
    for(std::size_t i = 0; i < loaded.size(); ++i) {
        Check(loaded[i].filePath == descriptions[i].filePath &&
                  Equal(loaded[i].position, descriptions[i].position) &&
                  Equal(loaded[i].rotation, descriptions[i].rotation) &&
                  Equal(loaded[i].scale, descriptions[i].scale),
              "Model transform round-trip changed values");
    }
    const auto unicodeConfigPath = root / std::filesystem::path(u8"sc\u00e8ne-\u573a\u666f.json");
    const ConfigManager unicodeConfig(unicodeConfigPath);
    auto unicodeDescription = first;
    unicodeDescription.filePath =
        PathToUtf8(root / std::filesystem::path(u8"caf\u00e9-\u6a21\u578b.obj"));
    const std::vector<ModelDescription> unicodeDescriptions{unicodeDescription};
    Check(unicodeConfig.SaveObject(unicodeDescriptions),
          "Cannot save configuration using non-ASCII paths");
    const auto unicodeLoaded = unicodeConfig.LoadObjectFromJson();
    Check(unicodeLoaded.size() == 1 && unicodeLoaded[0].filePath == unicodeDescription.filePath &&
              Equal(unicodeLoaded[0].scale, unicodeDescription.scale),
          "UTF-8 config round-trip changed paths or transforms");
    Check(ReadFile(unicodeConfigPath).find(unicodeDescription.filePath) != std::string::npos,
          "Persisted path is not UTF-8 JSON text");
    WriteFile(
        unicodeConfigPath,
        R"({"model":[{"filePath":"caf\u00e9-\u6a21\u578b.obj","position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}]})");
    const auto escapedUnicode = unicodeConfig.LoadObjectFromJson();
    Check(escapedUnicode.size() == 1 &&
              escapedUnicode[0].filePath ==
                  PathToUtf8(std::filesystem::path(u8"caf\u00e9-\u6a21\u578b.obj")),
          "Escaped JSON path did not decode to UTF-8");
    const auto previous = ReadFile(path);
    descriptions[0].position.x = std::numeric_limits<float>::quiet_NaN();
    Check(!config.SaveObject(descriptions), "Config saved a nonfinite transform");
    Check(ReadFile(path) == previous, "Failed save replaced the previous config");
    descriptions[0] = first;
    descriptions[0].filePath.clear();
    Check(!config.SaveObject(descriptions), "Config saved an empty file path");
    descriptions[0].filePath = std::string("bad\0path.obj", 12);
    Check(!config.SaveObject(descriptions), "Config saved an embedded NUL file path");
    Check(ReadFile(path) == previous, "Invalid path save replaced the previous config");
    Check(!ConfigManager(root / "missing-parent" / "scene.json").SaveObject(empty),
          "Save to missing parent reported success");
    const auto directory = root / "directory-target";
    WriteFile(directory / "sentinel", "keep me");
    Check(!ConfigManager(directory).SaveObject(empty), "Save over a directory reported success");
    Check(ReadFile(directory / "sentinel") == "keep me",
          "Failed replacement removed existing target data");
    CheckThrows([&] { ConfigManager(directory).LoadObjectFromJson(); },
                "Directory loaded as empty config");
    CheckThrows([] { ConfigManager(std::filesystem::path{}); }, "Empty config path accepted");

    const std::vector<std::string> malformed = {
        "{ broken json",
        "{}",
        "{\"model\":null}",
        "{\"model\":[7]}",
        R"({"model":[{"filePath":8,"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}]})",
        R"({"model":[{"filePath":"x.obj","position":[0,0],"rotation":[0,0,0],"scale":[1,1,1]}]})",
        R"({"model":[{"filePath":"x.obj","position":[false,0,0],"rotation":[0,0,0],"scale":[1,1,1]}]})",
        R"({"model":[{"filePath":"x.obj","position":[1e40,0,0],"rotation":[0,0,0],"scale":[1,1,1]}]})",
        R"({"model":[{"filePath":"x.obj","position":[0,0,0],"rotation":[0,0,0],"scale":null}]})"};
    for(const auto& text : malformed) {
        WriteFile(path, text);
        CheckThrows([&] { config.LoadObjectFromJson(); }, "Malformed configuration accepted");
        Check(ReadFile(path) == text, "Loading malformed JSON modified it");
    }
    Check(config.SaveObject(empty) && config.LoadObjectFromJson().empty(),
          "Final empty scene replacement failed");
    for(const auto& entry : std::filesystem::directory_iterator(root)) {
        Check(PathToUtf8(entry.path().filename()).find(".tmp-") == std::string::npos,
              "Config leaked a temporary directory");
    }
}

std::vector<PendingModelData> WaitForResults(ModelLoaderThread& worker, std::size_t count) {
    std::vector<PendingModelData> results;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while(results.size() < count && std::chrono::steady_clock::now() < deadline) {
        auto batch = worker.TakeCompleted(1);
        Check(batch.size() <= 1, "TakeCompleted ignored maxCount");
        if(batch.empty()) {
            std::this_thread::sleep_for(1ms);
        }
        else {
            results.push_back(std::move(batch.front()));
        }
    }
    Check(results.size() == count, "Timed out waiting for worker results");
    return results;
}

void TestWorker(const std::filesystem::path& root) {
    static_assert(!std::is_copy_constructible_v<ModelLoaderThread>);
    static_assert(!std::is_copy_assignable_v<ModelLoaderThread>);
    static_assert(std::is_nothrow_move_constructible_v<PendingModelData>);
    ImportSettings settings;
    settings.textureSize = 4;
    const auto path = root / std::filesystem::path(u8"worker-\u6a21\u578b.obj");
    WriteFile(path, Triangle);
    auto description = Describe(path);
    {
        ModelLoaderThread automaticShutdown(settings);
    }
    {
        ModelLoaderThread idle(settings);
        idle.Stop();
        idle.Stop();
        Check(!idle.QueueModelLoad(description, 1), "Stopped worker accepted a job");
        Check(idle.TakeCompleted(2).empty(), "Idle stopped worker has completions");
    }
    {
        ModelLoaderThread worker(settings);
        Check(worker.QueueModelLoad(description, 101), "Cannot queue valid model");
        Check(worker.QueueModelLoad(Describe(root / "not-found.obj"), 102),
              "Cannot queue failing model");
        description.position = {9.0f, 8.0f, 7.0f};
        Check(worker.QueueModelLoad(std::move(description), 103), "Cannot move a job into worker");
        Check(worker.TakeCompleted(0).empty(), "Zero-count take drained results");
        auto results = WaitForResults(worker, 3);
        ValidateModel(results[0], 4);
        Check(results[0].requestId == 101 && results[1].requestId == 102 &&
                  results[2].requestId == 103,
              "Worker lost request IDs or FIFO ordering");
        Check(!results[1].Success() && !results[1].error.empty(),
              "Worker failed to report an import error");
        Check(results[1].description.filePath == PathToUtf8(root / "not-found.obj"),
              "Failed result lost description");
        Check(Equal(results[2].description.position, {9.0f, 8.0f, 7.0f}),
              "Worker lost moved description");
        const auto* vertices = results[0].mesh.vertices.data();
        const auto* pixels = results[0].textures[0].pixels.data();
        auto moved = std::move(results[0]);
        Check(moved.mesh.vertices.data() == vertices && moved.textures[0].pixels.data() == pixels,
              "Moving a completion copied owned CPU buffers");
        worker.Stop();
        ValidateModel(moved, 4);
    }
    description = Describe(path);
    {
        ModelLoaderThread full(settings);
        for(ModelId id = 1; id <= 64; ++id) {
            Check(full.QueueModelLoad(description, id), "Cannot queue backpressure fixture");
        }
        std::this_thread::sleep_for(100ms);
        auto batch = full.TakeCompleted(100);
        Check(!batch.empty() && batch.size() <= 2,
              "Completed CPU queue exceeded its two-job bound");
        const auto nextId = batch.back().requestId + 1;
        const auto resumed = WaitForResults(full, 2);
        Check(resumed.front().requestId == nextId,
              "Draining the completed queue did not wake its producer");
        std::this_thread::sleep_for(100ms);
        const auto before = std::chrono::steady_clock::now();
        full.Stop();
        full.Stop();
        Check(std::chrono::steady_clock::now() - before < 5s,
              "Stop blocked on a full completed queue");
        Check(full.TakeCompleted(100).empty(), "Stop retained canceled CPU completions");
    }

    std::string busyMesh = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\n";
    busyMesh.reserve(2'400'000);
    for(int i = 0; i < 100'000; ++i) {
        busyMesh += "f 1//1 2//1 3//1\n";
    }
    const auto busyPath = root / "busy.obj";
    WriteFile(busyPath, busyMesh);
    ModelLoaderThread busy(settings);
    Check(busy.QueueModelLoad(Describe(busyPath), 500), "Cannot queue busy fixture");
    for(ModelId id = 501; id < 533; ++id) {
        Check(busy.QueueModelLoad(description, id), "Cannot queue cancellation fixture");
    }
    std::this_thread::sleep_for(5ms);
    auto firstStop = std::async(std::launch::async, [&] { busy.Stop(); });
    auto secondStop = std::async(std::launch::async, [&] { busy.Stop(); });
    Check(firstStop.wait_for(10s) == std::future_status::ready &&
              secondStop.wait_for(10s) == std::future_status::ready,
          "Concurrent busy Stop did not finish");
    firstStop.get();
    secondStop.get();
    Check(!busy.QueueModelLoad(description, 900), "Busy stopped worker accepted a new job");
    Check(busy.TakeCompleted(2).empty(), "Busy Stop retained canceled results");
}
} // namespace

int main() {
    try {
        TemporaryDirectory fixtures;
        TestImages(fixtures.path);
        std::cout << "Image tests passed\n";
        TestImporter(fixtures.path);
        std::cout << "Import tests passed\n";
        TestConfig(fixtures.path);
        std::cout << "Config tests passed\n";
        TestWorker(fixtures.path);
        std::cout << "Worker tests passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << "Asset test failure: " << error.what() << '\n';
        return 1;
    }
}
