#pragma once

#include "io/SceneTypes.h"

#include <array>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

glm::mat4 MakeModelMatrix(const ModelDescription& description);
// A positive common rescaling may be applied; zero means no usable normal transform.
glm::mat3 MakeNormalMatrix(const glm::mat4& modelMatrix);
bool IsMirrored(const glm::vec3& scale);
Bounds TransformBounds(const Bounds& bounds, const glm::mat4& transform);
bool IsFinite(const glm::vec3& value);

class Frustum {
public:
    explicit Frustum(const glm::mat4& viewProjection);
    bool Intersects(const Bounds& bounds) const;

private:
    std::array<glm::vec4, 6> planes;
};
