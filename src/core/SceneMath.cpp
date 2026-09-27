#include "core/SceneMath.h"

#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

bool IsFinite(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

glm::mat4 MakeModelMatrix(const ModelDescription& description) {
    if(!IsFinite(description.position) || !IsFinite(description.rotation) ||
       !IsFinite(description.scale)) {
        throw std::invalid_argument("Model transforms must be finite");
    }
    auto matrix = glm::translate(glm::mat4(1.0f), description.position);
    matrix = glm::rotate(matrix, glm::radians(description.rotation.x), glm::vec3(1, 0, 0));
    matrix = glm::rotate(matrix, glm::radians(description.rotation.y), glm::vec3(0, 1, 0));
    matrix = glm::rotate(matrix, glm::radians(description.rotation.z), glm::vec3(0, 0, 1));
    return glm::scale(matrix, description.scale);
}

Bounds TransformBounds(const Bounds& bounds, const glm::mat4& transform) {
    const glm::vec3 center = bounds.minimum * 0.5f + bounds.maximum * 0.5f;
    const glm::vec3 extent = bounds.maximum * 0.5f - bounds.minimum * 0.5f;
    const glm::vec3 worldCenter = glm::vec3(transform * glm::vec4(center, 1.0f));
    const glm::vec3 worldExtent = glm::abs(glm::vec3(transform[0])) * extent.x +
                                  glm::abs(glm::vec3(transform[1])) * extent.y +
                                  glm::abs(glm::vec3(transform[2])) * extent.z;
    Bounds result{worldCenter - worldExtent, worldCenter + worldExtent};
    if(!IsFinite(result.minimum) || !IsFinite(result.maximum)) {
        throw std::invalid_argument("Transformed model bounds exceed the supported range");
    }
    return result;
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
