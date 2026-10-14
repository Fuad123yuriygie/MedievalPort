#include "core/SceneMath.h"

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <stdexcept>

bool IsFinite(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

SceneLighting ValidateSceneLighting(SceneLighting lighting) {
    if(!IsFinite(lighting.direction) || !IsFinite(lighting.color) ||
       !std::isfinite(lighting.intensity) || !std::isfinite(lighting.ambientStrength)) {
        throw std::invalid_argument("Lighting must contain finite numbers");
    }
    for(int axis = 0; axis < 3; ++axis) {
        if(lighting.color[axis] < 0.0f || lighting.color[axis] > 1.0f) {
            throw std::invalid_argument("Lighting color must be in [0, 1]");
        }
    }
    if(lighting.intensity < 0.0f || lighting.intensity > 4.0f ||
       lighting.ambientStrength < 0.0f || lighting.ambientStrength > 1.0f) {
        throw std::invalid_argument("Lighting intensity must be in [0, 4] and ambient strength in [0, 1]");
    }
    const glm::dvec3 direction(lighting.direction);
    const double length = glm::length(direction);
    if(length == 0.0) {
        throw std::invalid_argument("Lighting direction must be nonzero");
    }
    // Already-normalized float state must stay stable across edits and save/load round trips.
    if(std::abs(length - 1.0) > static_cast<double>(std::numeric_limits<float>::epsilon()) * 0.5) {
        lighting.direction = glm::vec3(direction / length);
    }
    return lighting;
}

glm::mat4 MakeModelMatrix(const ModelDescription& description) {
    if(!IsFinite(description.position) || !IsFinite(description.rotation) ||
       !IsFinite(description.scale)) {
        throw std::invalid_argument("Model transforms must be finite");
    }
    auto rotation = glm::rotate(glm::dmat4(1.0),
                                glm::radians(static_cast<double>(description.rotation.x)),
                                glm::dvec3(1, 0, 0));
    rotation = glm::rotate(rotation, glm::radians(static_cast<double>(description.rotation.y)),
                           glm::dvec3(0, 1, 0));
    rotation = glm::rotate(rotation, glm::radians(static_cast<double>(description.rotation.z)),
                           glm::dvec3(0, 0, 1));
    glm::mat4 matrix(1.0f);
    for(int column = 0; column < 3; ++column) {
        for(int row = 0; row < 3; ++row) {
            // A rotation is bounded by one; clamp double roundoff before multiplying extreme scales.
            matrix[column][row] = static_cast<float>(
                std::clamp(rotation[column][row], -1.0, 1.0) *
                static_cast<double>(description.scale[column]));
        }
    }
    matrix[3] = glm::vec4(description.position, 1.0f);
    return matrix;
}

glm::mat3 MakeNormalMatrix(const glm::mat4& modelMatrix) {
    const glm::dmat3 linear(modelMatrix);
    for(int column = 0; column < 3; ++column) {
        if(!IsFinite(glm::vec3(modelMatrix[column]))) {
            return glm::mat3(0.0f);
        }
    }
    // Float inputs cannot overflow/underflow these double cofactor and determinant products.
    const glm::dmat3 cofactors(glm::cross(linear[1], linear[2]),
                              glm::cross(linear[2], linear[0]),
                              glm::cross(linear[0], linear[1]));
    const double determinant = glm::dot(linear[0], cofactors[0]);
    const auto usableDeterminant = [](const glm::dmat3& matrix, double value) {
        double magnitude = 0.0;
        for(int row = 0; row < 3; ++row) {
            const int next = (row + 1) % 3;
            const int last = (row + 2) % 3;
            magnitude += std::abs(matrix[0][row]) *
                         (std::abs(matrix[1][next] * matrix[2][last]) +
                          std::abs(matrix[1][last] * matrix[2][next]));
        }
        // Cancellation can leave a nonzero rounded determinant even for an exactly singular matrix.
        return std::isfinite(value) &&
               std::abs(value) > magnitude * (8.0 * std::numeric_limits<double>::epsilon());
    };
    if(!usableDeterminant(linear, determinant)) {
        return glm::mat3(0.0f);
    }
    const glm::dmat3 inverseTranspose = cofactors / determinant;
    double largest = 0.0;
    double smallest = std::numeric_limits<double>::max();
    for(int column = 0; column < 3; ++column) {
        for(int row = 0; row < 3; ++row) {
            if(!std::isfinite(inverseTranspose[column][row])) {
                return glm::mat3(0.0f);
            }
            const double magnitude = std::abs(inverseTranspose[column][row]);
            largest = std::max(largest, magnitude);
            if(magnitude > 0.0) {
                smallest = std::min(smallest, magnitude);
            }
        }
    }
    if(largest == 0.0) {
        return glm::mat3(0.0f);
    }
    // Leave matrix/vector multiply headroom at both ends of the normal float range when possible.
    const double maximum = static_cast<double>(std::numeric_limits<float>::max()) / 4.0;
    const double minimum = std::numeric_limits<float>::min();
    double factor = 1.0;
    if(largest > maximum || smallest < minimum * 4.0) {
        const double lowerFactor = (minimum * 4.0) / smallest;
        const double upperFactor = maximum / largest;
        factor = lowerFactor <= upperFactor ? std::clamp(1.0 / largest, lowerFactor, upperFactor)
                                            : upperFactor;
    }
    glm::mat3 result(0.0f);
    for(int column = 0; column < 3; ++column) {
        for(int row = 0; row < 3; ++row) {
            const double value = inverseTranspose[column][row] * factor;
            // Denormals can be flushed on the GPU; discard them before testing usability below.
            if(std::abs(value) >= minimum) {
                result[column][row] = static_cast<float>(value);
            }
        }
    }
    const glm::dmat3 narrowed(result);
    if(!usableDeterminant(narrowed, glm::determinant(narrowed))) {
        return glm::mat3(0.0f);
    }
    return result;
}

bool IsMirrored(const glm::vec3& scale) {
    if(!IsFinite(scale) || scale.x == 0.0f || scale.y == 0.0f || scale.z == 0.0f) {
        return false;
    }
    return (scale.x < 0.0f) != ((scale.y < 0.0f) != (scale.z < 0.0f));
}

Bounds TransformBounds(const Bounds& bounds, const glm::mat4& transform) {
    const glm::dvec3 center = (glm::dvec3(bounds.minimum) + glm::dvec3(bounds.maximum)) * 0.5;
    const glm::dvec3 extent = (glm::dvec3(bounds.maximum) - glm::dvec3(bounds.minimum)) * 0.5;
    const glm::dvec3 worldCenter = glm::dvec3(glm::dmat4(transform) * glm::dvec4(center, 1.0));
    const glm::dvec3 worldExtent = glm::abs(glm::dvec3(transform[0])) * extent.x +
                                   glm::abs(glm::dvec3(transform[1])) * extent.y +
                                   glm::abs(glm::dvec3(transform[2])) * extent.z;
    const auto minimum = worldCenter - worldExtent;
    const auto maximum = worldCenter + worldExtent;
    for(int axis = 0; axis < 3; ++axis) {
        if(!std::isfinite(minimum[axis]) || !std::isfinite(maximum[axis]) ||
           std::abs(minimum[axis]) > std::numeric_limits<float>::max() ||
           std::abs(maximum[axis]) > std::numeric_limits<float>::max()) {
            throw std::invalid_argument("Transformed model bounds exceed the supported range");
        }
    }
    return {glm::vec3(minimum), glm::vec3(maximum)};
}

Frustum::Frustum(const glm::mat4& viewProjection) {
    // GLM is column-major; clip-space plane extraction uses matrix rows.
    const auto rows = glm::transpose(viewProjection);
    planes = {rows[3] + rows[0],
              rows[3] - rows[0],
              rows[3] + rows[1],
              rows[3] - rows[1],
              rows[3] + rows[2],
              rows[3] - rows[2]};
}

bool Frustum::Intersects(const Bounds& bounds) const {
    for(const auto& plane : planes) {
        const glm::vec3 positiveVertex{plane.x >= 0 ? bounds.maximum.x : bounds.minimum.x,
                                       plane.y >= 0 ? bounds.maximum.y : bounds.minimum.y,
                                       plane.z >= 0 ? bounds.maximum.z : bounds.minimum.z};
        if(glm::dot(glm::vec3(plane), positiveVertex) + plane.w < 0.0f) {
            return false;
        }
    }
    return true;
}
