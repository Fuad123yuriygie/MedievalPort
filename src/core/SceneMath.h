#pragma once

#include "io/LoadData.h"

#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

glm::mat4 MakeModelMatrix(const ModelDescription& description);
Bounds TransformBounds(const Bounds& bounds, const glm::mat4& transform);
bool IsFinite(const glm::vec3& value);

class Frustum {
public:
    explicit Frustum(const glm::mat4& viewProjection);
    bool Intersects(const Bounds& bounds) const;

private:
    std::array<glm::vec4, 6> planes;
};
