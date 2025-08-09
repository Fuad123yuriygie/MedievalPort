#include "LoadData.h"
#include <tinyobj/tiny_obj_loader.h>

// Function to load an OBJ file
bool LoadObject(const std::string& filePath, std::vector<float>& vertices,
                std::vector<unsigned int>& indices, std::vector<std::string>& diffuseTextures,
                std::vector<int>& materialIndices) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    std::string mtlPath = std::filesystem::path(filePath).parent_path().string();

    if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str(), mtlPath.c_str())) {
        std::cerr << "Failed to load OBJ file: " << warn << err << std::endl;
        return false;
    }

    // Extract texture names
    for(const auto& material : materials) {
        if (!material.diffuse_texname.empty()) {
            // Only the file name and extension, prepend the mtlPath
            std::string texPath = mtlPath + "/" + std::filesystem::path(material.diffuse_texname).filename().string();
            diffuseTextures.push_back(texPath);
        } else {
            diffuseTextures.push_back(""); // Or a default/fallback texture path if you prefer
        }
    }

    // Process the shapes
    unsigned int vertexCount = 0;
    for(const auto& shape : shapes) {
        size_t index_offset = 0; // <-- Reset for each shape
        for(size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            int mat_id = shape.mesh.material_ids[f];
            int fv = shape.mesh.num_face_vertices[f];
            for(size_t v = 0; v < fv; v++) {
                // Retrieve the index for this vertex
                tinyobj::index_t index = shape.mesh.indices[index_offset + v];

                // Extract vertex positions
                vertices.push_back(attrib.vertices[3 * index.vertex_index + 0]); // x
                vertices.push_back(attrib.vertices[3 * index.vertex_index + 1]); // y
                vertices.push_back(attrib.vertices[3 * index.vertex_index + 2]); // z

                // Extract vertex normals (if available)
                if(!attrib.normals.empty()) {
                    vertices.push_back(attrib.normals[3 * index.normal_index + 0]); // nx
                    vertices.push_back(attrib.normals[3 * index.normal_index + 1]); // ny
                    vertices.push_back(attrib.normals[3 * index.normal_index + 2]); // nz
                }
                else {
                    // If no normals, add placeholders
                    vertices.push_back(0.0f); // nx
                    vertices.push_back(0.0f); // ny
                    vertices.push_back(0.0f); // nz
                }

                // Extract vertex texture coordinates (if available)
                if(!attrib.texcoords.empty()) {
                    vertices.push_back(attrib.texcoords[2 * index.texcoord_index + 0]); // u
                    vertices.push_back(attrib.texcoords[2 * index.texcoord_index + 1]); // v
                }
                else {
                    // If no texture coordinates, add placeholders
                    vertices.push_back(0.0f); // u
                    vertices.push_back(0.0f); // v
                }

                materialIndices.push_back(mat_id);
                // MaterialIndices.push_back(0);

                // Push the index
                indices.push_back(vertexCount);
                vertexCount++;
            }
            index_offset += fv;
        }
    }

    return true;
}
