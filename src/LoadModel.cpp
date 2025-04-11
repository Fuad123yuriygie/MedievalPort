#include "LoadModel.h"
#include "tinyobj/tiny_obj_loader.h"

// Function to load an OBJ file
bool LoadObject(const std::string& filePath, std::vector<float>& vertices, std::vector<unsigned int>& indices) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // Load the OBJ file
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str())) {
        std::cout << "Failed to load OBJ file: " << warn << err << std::endl;
        return false;
    }

    // Process the shapes
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            // Extract vertex positions
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 0]); // x
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 1]); // y
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 2]); // z

            // Extract vertex normals (if available)
            if (!attrib.normals.empty()) {
                vertices.push_back(attrib.normals[3 * index.normal_index + 0]); // nx
                vertices.push_back(attrib.normals[3 * index.normal_index + 1]); // ny
                vertices.push_back(attrib.normals[3 * index.normal_index + 2]); // nz
            } else {
                // If no normals, add placeholders
                vertices.push_back(0.0f); // nx
                vertices.push_back(0.0f); // ny
                vertices.push_back(0.0f); // nz
            }

            // Extract vertex texture coordinates (if available)
            if (!attrib.texcoords.empty()) {
                vertices.push_back(attrib.texcoords[2 * index.texcoord_index + 0]); // u
                vertices.push_back(attrib.texcoords[2 * index.texcoord_index + 1]); // v
            } else {
                // If no texture coordinates, add placeholders
                vertices.push_back(0.0f); // u
                vertices.push_back(0.0f); // v
            }

            // Add the index
            indices.push_back(indices.size());
        }
    }

    return true;
}
