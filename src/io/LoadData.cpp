#include "io/LoadData.h"
#include "io/PathUtils.h"
#include "utils/Log.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobj/tiny_obj_loader.h>

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <numeric>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace
{
constexpr std::size_t MaxDrawIndices = std::numeric_limits<std::int32_t>::max();

std::string_view NextToken(std::string_view& line) {
    const auto start = line.find_first_not_of(" \t\r");
    if(start == std::string_view::npos) {
        line = {};
        return {};
    }
    line.remove_prefix(start);
    const auto end = line.find_first_of(" \t\r");
    const auto token = line.substr(0, end);
    line.remove_prefix(end == std::string_view::npos ? line.size() : end);
    return token;
}

std::size_t ValidateObjSource(std::istream& input, std::string& validatedSource) {
    std::array<std::size_t, 3> counts{}; // position, UV, normal
    std::array<int, 3> greatestIndex{};
    std::size_t expectedIndices = 0;
    std::size_t lineNumber = 0;
    std::string text;
    while(std::getline(input, text)) {
        ++lineNumber;
        // Do not let embedded CR characters introduce unchecked records in tinyobj's line reader.
        std::replace(text.begin(), text.end(), '\r', ' ');
        std::string_view line(text);
        line = line.substr(0, line.find('#'));
        const auto record = line;
        const auto kind = NextToken(line);
        const auto invalid = [lineNumber](const char* reason) {
            throw std::runtime_error(std::string(reason) + " at OBJ line " +
                                     std::to_string(lineNumber));
        };
        if(kind == "v" || kind == "vn" || kind == "vt") {
            std::size_t components = 0;
            for(auto token = NextToken(line); !token.empty(); token = NextToken(line)) {
                if(token.front() == '+') {
                    token.remove_prefix(1);
                    if(token.empty() || token.front() == '-' || token.front() == '+') {
                        invalid("Invalid attribute sign");
                    }
                }
                double value = 0.0;
                const auto parsed =
                    std::from_chars(token.data(), token.data() + token.size(), value);
                if(parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() ||
                   !std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) {
                    invalid("Invalid or nonfinite attribute");
                }
                ++components;
            }
            if((kind == "v" && components != 3 && components != 4 && components != 6 &&
                components != 7) ||
               (kind == "vn" && components != 3) ||
               (kind == "vt" && (components < 1 || components > 3))) {
                invalid("Invalid attribute component count");
            }
            const auto attribute = kind == "v" ? 0u : (kind == "vt" ? 1u : 2u);
            if(++counts[attribute] > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                invalid("Too many OBJ attributes");
            }
        }
        else if(kind == "f" || kind == "l" || kind == "p") {
            std::size_t corners = 0;
            for(auto token = NextToken(line); !token.empty(); token = NextToken(line)) {
                std::size_t attribute = 0;
                for(;;) {
                    if(attribute >= counts.size()) {
                        invalid("Too many corner index fields");
                    }
                    const auto slash = token.find('/');
                    auto field = token.substr(0, slash);
                    if(field.empty()) {
                        if(attribute == 0) {
                            invalid("Missing position index");
                        }
                    }
                    else {
                        if(field.front() == '+') {
                            field.remove_prefix(1);
                            if(field.empty() || field.front() == '-' || field.front() == '+') {
                                invalid("Invalid corner index sign");
                            }
                        }
                        int index = 0;
                        const auto parsed =
                            std::from_chars(field.data(), field.data() + field.size(), index);
                        if(parsed.ec != std::errc{} || parsed.ptr != field.data() + field.size() ||
                           index == 0) {
                            invalid("Invalid corner index");
                        }
                        if(index < 0 && -static_cast<std::int64_t>(index) >
                                            static_cast<std::int64_t>(counts[attribute])) {
                            invalid("Relative corner index out of range");
                        }
                        greatestIndex[attribute] = std::max(greatestIndex[attribute], index);
                    }
                    if(slash == std::string_view::npos) {
                        break;
                    }
                    token.remove_prefix(slash + 1);
                    ++attribute;
                }
                ++corners;
            }
            if(corners < (kind == "f" ? 3u : (kind == "l" ? 2u : 1u))) {
                invalid("Insufficient primitive corners");
            }
            if(kind == "f") {
                if(corners - 2 > (MaxDrawIndices - expectedIndices) / 3) {
                    invalid("Mesh exceeds the signed draw index limit");
                }
                expectedIndices += (corners - 2) * 3;
            }
        }
        else if(kind == "mtllib") {
            std::string filename;
            const auto appendLibrary = [&] {
                if(!filename.empty()) {
                    // tinyobj stops after the first existing library on a single mtllib line.
                    validatedSource += "mtllib ";
                    for(const char c : filename) {
                        if(c == ' ' || c == '\t') {
                            validatedSource += '\\';
                        }
                        validatedSource += c;
                    }
                    validatedSource += '\n';
                    filename.clear();
                }
            };
            for(std::size_t i = 0; i < line.size(); ++i) {
                const char c = line[i];
                if(c == '\\') {
                    // Backslashes are directory separators except when escaping filename
                    // whitespace.
                    if(i + 1 < line.size() && (line[i + 1] == ' ' || line[i + 1] == '\t')) {
                        filename += line[++i];
                    }
                    else {
                        filename += '/';
                    }
                }
                else if(c == ' ' || c == '\t') {
                    appendLibrary();
                }
                else {
                    filename += c;
                }
            }
            appendLibrary();
            continue;
        }
        else if(kind != "o" && kind != "g" && kind != "usemtl") {
            // Smoothing groups, subdivision tags, skin weights and unknown records are unused.
            continue;
        }
        validatedSource.append(record);
        validatedSource += '\n';
    }
    if(input.bad()) {
        throw std::runtime_error("Cannot read OBJ file");
    }
    for(std::size_t i = 0; i < counts.size(); ++i) {
        if(static_cast<std::size_t>(greatestIndex[i]) > counts[i]) {
            throw std::runtime_error("OBJ corner index out of range");
        }
    }
    return expectedIndices;
}

std::string ValidateMaterialSource(std::istream& input) {
    std::string validated;
    std::string text;
    while(std::getline(input, text)) {
        std::replace(text.begin(), text.end(), '\r', ' ');
        std::string_view record(text);
        record = record.substr(0, record.find('#'));
        const auto last = record.find_last_not_of(" \t");
        if(last == std::string_view::npos) {
            continue;
        }
        record = record.substr(0, last + 1);
        std::vector<std::string_view> tokens;
        auto remainder = record;
        for(auto token = NextToken(remainder); !token.empty(); token = NextToken(remainder)) {
            tokens.push_back(token);
        }
        if(tokens.empty() || (tokens[0] != "newmtl" && tokens[0] != "map_Kd")) {
            // Other material fields are not rendered, and tinyobj parses some with unchecked atoi.
            continue;
        }
        if(tokens[0] != "map_Kd") {
            validated.append(record);
            validated += '\n';
            continue;
        }

        // tinyobj consumes a fixed token count per numeric option and uses unchecked atoi for
        // -texres, so an unrepresentable value must be rejected before the vendor parser reaches
        // it.
        const auto reject = [](std::string_view option) {
            throw std::runtime_error("Unrepresentable diffuse texture option: " +
                                     std::string(option));
        };
        std::size_t index = 1;
        while(index < tokens.size()) {
            const auto option = tokens[index];
            if(option.empty() || option.front() != '-') {
                break; // The filename, which may contain spaces, ends option parsing.
            }
            ++index;
            const int numbers = option == "-o" || option == "-s" || option == "-t"             ? 3
                                : option == "-mm"                                              ? 2
                                : option == "-boost" || option == "-bm" || option == "-texres" ? 1
                                                                                               : 0;
            if(numbers > 0) {
                const auto available = static_cast<int>(
                    std::min<std::size_t>(index + static_cast<std::size_t>(numbers),
                                          tokens.size()) -
                    index);
                if(available < numbers) {
                    reject(option);
                }
                for(int value = 0; value < numbers; ++value, ++index) {
                    auto token = tokens[index];
                    if(token.starts_with('+')) {
                        token.remove_prefix(1);
                    }
                    if(option == "-texres") {
                        int resolution = 0;
                        const auto parsed =
                            std::from_chars(token.data(), token.data() + token.size(), resolution);
                        if(parsed.ec == std::errc::result_out_of_range) {
                            reject(option);
                        }
                    }
                    else {
                        double number = 0;
                        const auto parsed =
                            std::from_chars(token.data(), token.data() + token.size(), number);
                        if(parsed.ec == std::errc::result_out_of_range ||
                           (parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size() &&
                            (!std::isfinite(number) ||
                             std::abs(number) > std::numeric_limits<float>::max()))) {
                            reject(option);
                        }
                    }
                }
                continue;
            }
            if(option == "-blendu" || option == "-blendv" || option == "-clamp" ||
               option == "-type" || option == "-imfchan" || option == "-colorspace") {
                if(index >= tokens.size()) {
                    reject(option);
                }
                ++index;
                continue;
            }
            // Unknown options and their arguments fall through to the filename, as in tinyobj.
            break;
        }
        validated.append(record);
        validated += '\n';
    }
    if(input.bad()) {
        throw std::runtime_error("Cannot read material library");
    }
    return validated;
}

class RelativeMaterialReader final : public tinyobj::MaterialReader {
public:
    explicit RelativeMaterialReader(std::filesystem::path directory)
        : directory_(std::move(directory)) {
    }

    bool operator()(const std::string& name, std::vector<tinyobj::material_t>* materials,
                    std::map<std::string, int>* materialMap, std::string* warning,
                    std::string* error) override {
        const auto path = (directory_ / PathFromUtf8(name)).lexically_normal();
        std::ifstream input(path);
        if(!input) {
            if(warning) {
                *warning += "Cannot open material library: " + PathToUtf8(path) + '\n';
            }
            return false;
        }
        const auto firstMaterial = materials->size();
        std::string validatedSource;
        try {
            validatedSource = ValidateMaterialSource(input);
        } catch(const std::exception& failure) {
            // Dropping one library keeps the model usable with default materials, as tinyobj does
            // for a missing library, without ever handing the bad values to the vendor parser.
            if(warning) {
                *warning +=
                    "Ignoring material library " + PathToUtf8(path) + ": " + failure.what() + '\n';
            }
            return false;
        }
        std::istringstream validatedInput(validatedSource);
        tinyobj::LoadMtl(materialMap, materials, &validatedInput, warning, error);
        for(std::size_t i = firstMaterial; i < materials->size(); ++i) {
            auto& texture = (*materials)[i].diffuse_texname;
            if(!texture.empty()) {
                std::string portableTexture;
                for(std::size_t j = 0; j < texture.size(); ++j) {
                    if(texture[j] == '\\') {
                        if(j + 1 < texture.size() &&
                           (texture[j + 1] == ' ' || texture[j + 1] == '\t')) {
                            portableTexture += texture[++j];
                        }
                        else {
                            portableTexture += '/';
                        }
                    }
                    else {
                        portableTexture += texture[j];
                    }
                }
                texture = PathToUtf8(
                    (path.parent_path() / PathFromUtf8(portableTexture)).lexically_normal());
            }
        }
        return true;
    }

private:
    std::filesystem::path directory_;
};

glm::dvec3 Position(const tinyobj::attrib_t& attributes, int index) {
    const auto offset = static_cast<std::size_t>(index) * 3;
    return {attributes.vertices[offset],
            attributes.vertices[offset + 1],
            attributes.vertices[offset + 2]};
}

double Cross2(const glm::dvec2& a, const glm::dvec2& b) {
    return a.x * b.y - a.y * b.x;
}

void TriangulateFace(std::span<const tinyobj::index_t> corners, const tinyobj::attrib_t& attributes,
                     const glm::dvec3& normal, std::vector<std::size_t>& output) {
    output.clear();
    if(corners.size() == 3) {
        output.insert(output.end(), {0, 1, 2});
        return;
    }
    const auto absoluteNormal = glm::abs(normal);
    const int drop = absoluteNormal.x > absoluteNormal.y
                         ? (absoluteNormal.x > absoluteNormal.z ? 0 : 2)
                         : (absoluteNormal.y > absoluteNormal.z ? 1 : 2);
    const int x = drop == 0 ? 1 : 0;
    const int y = drop == 2 ? 1 : 2;
    const auto origin = Position(attributes, corners.front().vertex_index);
    std::vector<glm::dvec2> points;
    points.reserve(corners.size());
    for(const auto& corner : corners) {
        const auto p = Position(attributes, corner.vertex_index) - origin;
        points.emplace_back(p[x], p[y]);
    }
    double area = 0.0;
    for(std::size_t i = 0; i < points.size(); ++i) {
        area += Cross2(points[i], points[(i + 1) % points.size()]);
    }
    if(area == 0.0 || !std::isfinite(area)) {
        throw std::runtime_error("Degenerate OBJ polygon");
    }
    const double winding = area > 0.0 ? 1.0 : -1.0;
    const auto turn = [&](std::size_t a, std::size_t b, std::size_t c) {
        return winding * Cross2(points[b] - points[a], points[c] - points[a]);
    };
    bool convex = true;
    for(std::size_t i = 0; i < points.size(); ++i) {
        convex = convex && turn(i, (i + 1) % points.size(), (i + 2) % points.size()) > 0.0;
    }
    output.reserve((corners.size() - 2) * 3);
    if(convex) {
        for(std::size_t i = 1; i + 1 < points.size(); ++i) {
            if(turn(0, i, i + 1) <= 0.0) {
                throw std::runtime_error("Invalid OBJ polygon winding");
            }
            output.insert(output.end(), {0, i, i + 1});
        }
        return;
    }

    // Ear clipping preserves concave polygon boundaries; a simple fan does not.
    std::vector<std::size_t> remaining(points.size());
    std::iota(remaining.begin(), remaining.end(), std::size_t{0});
    while(remaining.size() > 3) {
        bool clipped = false;
        for(std::size_t i = 0; i < remaining.size(); ++i) {
            const auto a = remaining[(i + remaining.size() - 1) % remaining.size()];
            const auto b = remaining[i];
            const auto c = remaining[(i + 1) % remaining.size()];
            if(turn(a, b, c) <= 0.0) {
                continue;
            }
            bool occupied = false;
            for(const auto other : remaining) {
                if(other != a && other != b && other != c && turn(a, b, other) >= 0.0 &&
                   turn(b, c, other) >= 0.0 && turn(c, a, other) >= 0.0) {
                    occupied = true;
                    break;
                }
            }
            if(!occupied) {
                output.insert(output.end(), {a, b, c});
                remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
                clipped = true;
                break;
            }
        }
        if(!clipped) {
            throw std::runtime_error("Cannot triangulate malformed OBJ polygon");
        }
    }
    if(turn(remaining[0], remaining[1], remaining[2]) <= 0.0) {
        throw std::runtime_error("Degenerate triangle in OBJ polygon");
    }
    output.insert(output.end(), remaining.begin(), remaining.end());
}

struct VertexKey {
    int position;
    int normal;
    int uv;
    int material;
    std::size_t flatFace;
    bool operator==(const VertexKey&) const = default;
};

struct VertexKeyHash {
    std::size_t operator()(const VertexKey& key) const noexcept {
        std::size_t hash = 0;
        for(const auto value : {static_cast<std::size_t>(key.position),
                                static_cast<std::size_t>(key.normal),
                                static_cast<std::size_t>(key.uv),
                                static_cast<std::size_t>(key.material),
                                key.flatFace}) {
            hash ^= value + 0x9e3779b9u + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};
} // namespace

PendingModelData LoadModelData(ModelDescription description, const ImportSettings& settings) {
    PendingModelData result;
    result.description = std::move(description);
    try {
        if(result.description.filePath.empty() ||
           result.description.filePath.find('\0') != std::string::npos) {
            throw std::invalid_argument("Model file path is empty or contains NUL characters");
        }
        for(int i = 0; i < 3; ++i) {
            if(!std::isfinite(result.description.position[i]) ||
               !std::isfinite(result.description.rotation[i]) ||
               !std::isfinite(result.description.scale[i])) {
                throw std::invalid_argument("Model transforms must contain finite numbers");
            }
        }
        const auto requestedPath = PathFromUtf8(result.description.filePath);
        result.description.filePath = PathToUtf8(requestedPath);
        auto extension = PathToUtf8(requestedPath.extension());
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if(extension != ".obj") {
            throw std::invalid_argument("Only .obj model files are supported");
        }
        const auto path =
            std::filesystem::weakly_canonical(std::filesystem::absolute(requestedPath));
        std::ifstream input(path);
        if(!input) {
            throw std::runtime_error("Cannot open OBJ file: " + PathToUtf8(path));
        }
        // tinyobj accepts malformed numbers and truncates overflowing indices, so validate its
        // input first.
        std::string validatedSource;
        const auto expectedIndices = ValidateObjSource(input, validatedSource);
        if(expectedIndices == 0) {
            throw std::runtime_error("OBJ contains no triangle faces");
        }
        std::istringstream validatedInput(std::move(validatedSource));
        tinyobj::attrib_t attributes;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        RelativeMaterialReader materialReader(path.parent_path());
        std::string warning;
        std::string error;
        // Keep original faces until every corner is validated, including corners a triangulator
        // might drop.
        if(!tinyobj::LoadObj(&attributes,
                             &shapes,
                             &materials,
                             &warning,
                             &error,
                             &validatedInput,
                             &materialReader,
                             false,
                             false) ||
           !error.empty()) {
            throw std::runtime_error(error.empty() ? "Cannot parse OBJ file" : error);
        }
        if(!warning.empty()) {
            Log(LogLevel::Warning, warning);
        }
        if(attributes.vertices.size() % 3 != 0 || attributes.normals.size() % 3 != 0 ||
           attributes.texcoords.size() % 2 != 0 || materials.size() > MaxDrawIndices) {
            throw std::runtime_error("Invalid OBJ attribute or material array");
        }
        for(const auto* values :
            {&attributes.vertices, &attributes.normals, &attributes.texcoords}) {
            for(const auto value : *values) {
                if(!std::isfinite(value)) {
                    throw std::runtime_error("OBJ contains nonfinite attributes");
                }
            }
        }

        auto& mesh = result.mesh;
        std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash> vertices;
        vertices.reserve(std::min(expectedIndices, attributes.vertices.size() / 3));
        mesh.vertices.reserve(std::min(expectedIndices, attributes.vertices.size() / 3));
        std::map<int, std::vector<std::uint32_t>> materialIndices;
        std::vector<std::size_t> triangles;
        std::size_t faceNumber = 0;
        std::size_t indexCount = 0;
        for(const auto& shape : shapes) {
            const auto& source = shape.mesh;
            if(source.material_ids.size() != source.num_face_vertices.size()) {
                throw std::runtime_error("Invalid OBJ face material array");
            }
            std::size_t offset = 0;
            for(std::size_t face = 0; face < source.num_face_vertices.size(); ++face) {
                ++faceNumber;
                const auto count = static_cast<std::size_t>(source.num_face_vertices[face]);
                if(count < 3 || offset > source.indices.size() ||
                   count > source.indices.size() - offset ||
                   count - 2 > (MaxDrawIndices - indexCount) / 3) {
                    throw std::runtime_error("Invalid or oversized OBJ face");
                }
                const int material = source.material_ids[face];
                if(material < -1 ||
                   (material >= 0 && static_cast<std::size_t>(material) >= materials.size())) {
                    throw std::runtime_error("OBJ material index out of range");
                }
                const std::span corners(source.indices.data() + offset, count);
                for(const auto& corner : corners) {
                    if(corner.vertex_index < 0 ||
                       static_cast<std::size_t>(corner.vertex_index) >=
                           attributes.vertices.size() / 3 ||
                       corner.normal_index < -1 ||
                       (corner.normal_index >= 0 && static_cast<std::size_t>(corner.normal_index) >=
                                                        attributes.normals.size() / 3) ||
                       corner.texcoord_index < -1 ||
                       (corner.texcoord_index >= 0 &&
                        static_cast<std::size_t>(corner.texcoord_index) >=
                            attributes.texcoords.size() / 2)) {
                        throw std::runtime_error("OBJ corner attribute index out of range");
                    }
                }
                const auto origin = Position(attributes, corners.front().vertex_index);
                glm::dvec3 areaNormal(0.0);
                for(std::size_t i = 1; i + 1 < count; ++i) {
                    areaNormal +=
                        glm::cross(Position(attributes, corners[i].vertex_index) - origin,
                                   Position(attributes, corners[i + 1].vertex_index) - origin);
                }
                const double length = glm::length(areaNormal);
                if(!std::isfinite(length) || length == 0.0) {
                    throw std::runtime_error("Degenerate OBJ face has no valid normal");
                }
                const glm::vec3 flatNormal(areaNormal / length);
                TriangulateFace(corners, attributes, areaNormal, triangles);
                if(triangles.size() != (count - 2) * 3) {
                    throw std::runtime_error("Incomplete OBJ triangulation");
                }
                auto& group = materialIndices[material];
                for(const auto triangleCorner : triangles) {
                    const auto& corner = corners[triangleCorner];
                    const VertexKey key{corner.vertex_index,
                                        corner.normal_index,
                                        corner.texcoord_index,
                                        material,
                                        corner.normal_index == -1 ? faceNumber : 0};
                    auto found = vertices.find(key);
                    if(found == vertices.end()) {
                        if(mesh.vertices.size() >= std::numeric_limits<std::uint32_t>::max() ||
                           mesh.vertices.size() >= static_cast<std::size_t>(
                                                       std::numeric_limits<std::ptrdiff_t>::max()) /
                                                       sizeof(MeshVertex)) {
                            throw std::runtime_error(
                                "OBJ vertex buffer exceeds index or address limits");
                        }
                        MeshVertex vertex{glm::vec3(Position(attributes, corner.vertex_index)),
                                          flatNormal,
                                          glm::vec2(0.0f)};
                        if(corner.normal_index != -1) {
                            const auto base = static_cast<std::size_t>(corner.normal_index) * 3;
                            const glm::dvec3 normal(attributes.normals[base],
                                                    attributes.normals[base + 1],
                                                    attributes.normals[base + 2]);
                            const double normalLength = glm::length(normal);
                            if(!std::isfinite(normalLength) || normalLength == 0.0) {
                                throw std::runtime_error("OBJ contains a zero or invalid normal");
                            }
                            vertex.normal = glm::vec3(normal / normalLength);
                        }
                        if(corner.texcoord_index != -1) {
                            const auto base = static_cast<std::size_t>(corner.texcoord_index) * 2;
                            vertex.texCoord = {attributes.texcoords[base],
                                               attributes.texcoords[base + 1]};
                        }
                        if(mesh.vertices.empty()) {
                            mesh.bounds.minimum = vertex.position;
                            mesh.bounds.maximum = vertex.position;
                        }
                        else {
                            mesh.bounds.minimum = glm::min(mesh.bounds.minimum, vertex.position);
                            mesh.bounds.maximum = glm::max(mesh.bounds.maximum, vertex.position);
                        }
                        const auto index = static_cast<std::uint32_t>(mesh.vertices.size());
                        mesh.vertices.push_back(vertex);
                        found = vertices.emplace(key, index).first;
                    }
                    group.push_back(found->second);
                }
                indexCount += triangles.size();
                offset += count;
            }
            if(offset != source.indices.size()) {
                throw std::runtime_error("OBJ face ranges do not cover the index array");
            }
        }
        if(indexCount != expectedIndices ||
           indexCount > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) /
                            sizeof(std::uint32_t)) {
            throw std::runtime_error("Invalid OBJ index buffer size or triangulation");
        }

        result.textures.push_back(MakeSolidImage("builtin:white", settings.textureSize));
        std::unordered_map<std::string, std::uint32_t> textureIndices;
        mesh.indices.reserve(indexCount);
        for(const auto& [material, indices] : materialIndices) {
            std::uint32_t textureIndex = 0;
            if(material >= 0 && !materials[material].diffuse_texname.empty()) {
                const auto& textureName = materials[material].diffuse_texname;
                try {
                    const auto texturePath = std::filesystem::weakly_canonical(
                        std::filesystem::absolute(PathFromUtf8(textureName)));
                    const auto key = PathToUtf8(texturePath);
                    const auto found = textureIndices.find(key);
                    if(found != textureIndices.end()) {
                        textureIndex = found->second;
                    }
                    else {
                        // Cache failures as slice zero too, so a corrupt shared image is attempted
                        // only once.
                        auto entry = textureIndices.emplace(key, 0).first;
                        auto image = DecodeImage(texturePath,
                                                 true,
                                                 settings.textureSize,
                                                 settings.textureSize);
                        if(result.textures.size() >= std::numeric_limits<std::uint32_t>::max()) {
                            throw std::runtime_error("Too many model textures");
                        }
                        textureIndex = static_cast<std::uint32_t>(result.textures.size());
                        result.textures.push_back(std::move(image));
                        entry->second = textureIndex;
                    }
                } catch(const std::exception& textureError) {
                    textureIndex = 0;
                    Log(LogLevel::Warning,
                        "Using default texture for " + textureName + ": " + textureError.what());
                }
            }
            if(textureIndex >= result.textures.size() || indices.empty() ||
               indices.size() % 3 != 0) {
                throw std::runtime_error("Invalid submesh texture or index range");
            }
            mesh.submeshes.push_back({static_cast<std::uint32_t>(mesh.indices.size()),
                                      static_cast<std::uint32_t>(indices.size()),
                                      textureIndex});
            mesh.indices.insert(mesh.indices.end(), indices.begin(), indices.end());
        }
    } catch(const std::exception& error) {
        result.mesh = {};
        result.textures.clear();
        result.error = error.what();
        Log(LogLevel::Error, result.error);
    } catch(...) {
        result.mesh = {};
        result.textures.clear();
        result.error = "Unknown model import failure";
        Log(LogLevel::Error, result.error);
    }
    return result;
}
