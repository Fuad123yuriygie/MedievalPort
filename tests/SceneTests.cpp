#include "core/Application.h"
#include "core/Camera.h"
#include "core/SceneMath.h"
#include "graphics/IndexBuffer.h"
#include "graphics/Shader.h"
#include "graphics/TextureArray.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"
#include "graphics/VertexBufferLayout.h"

#include <array>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

static_assert(std::has_virtual_destructor_v<Application> && std::is_abstract_v<Application>);
static_assert(!std::is_copy_constructible_v<VertexBuffer> &&
              std::is_nothrow_move_constructible_v<VertexBuffer>);
static_assert(!std::is_copy_assignable_v<IndexBuffer> &&
              std::is_nothrow_move_assignable_v<IndexBuffer>);
static_assert(!std::is_copy_constructible_v<VertexArray> &&
              std::is_nothrow_move_constructible_v<VertexArray>);
static_assert(!std::is_copy_constructible_v<TextureArray> &&
              std::is_nothrow_move_constructible_v<TextureArray>);
static_assert(!std::is_copy_constructible_v<Shader> &&
              std::is_nothrow_move_constructible_v<Shader>);

namespace
{
void Check(bool condition, const char* message) {
    if(!condition) {
        throw std::runtime_error(message);
    }
}

bool Near(float left, float right) {
    return std::abs(left - right) < 0.0001f;
}

template <class Function> void CheckInvalid(Function&& function, const char* message) {
    try {
        function();
    } catch(const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(message);
}

void CheckZeroNormalMatrix(const glm::mat3& matrix) {
    for(int column = 0; column < 3; ++column) {
        for(int row = 0; row < 3; ++row) {
            Check(matrix[column][row] == 0.0f, "Unusable transform did not return a zero normal matrix");
        }
    }
}

void TestCamera() {
    Camera camera;
    Camera independent;
    camera.Resize(1280, 720);
    const auto projection = camera.GetProjection();
    camera.Resize(0, 0);
    camera.Resize(100, 0);
    camera.Resize(-1, 100);
    for(int column = 0; column < 4; ++column) {
        for(int row = 0; row < 4; ++row) {
            Check(std::isfinite(camera.GetProjection()[column][row]) &&
                      camera.GetProjection()[column][row] == projection[column][row],
                  "Minimized resize changed the last valid projection");
        }
    }
    camera.Move({0, 0, 1}, 20.0f);
    Check(Near(camera.GetPosition().z, 2.75f), "Camera failed to clamp a timestep spike");
    Check(Near(independent.GetPosition().z, 3.0f), "Camera instances share mutable state");
    camera.Move({0, 0, 1}, -1.0f);
    camera.Move({0, 0, 1}, std::numeric_limits<float>::quiet_NaN());
    Check(Near(camera.GetPosition().z, 2.75f), "Camera accepted an invalid timestep");
    camera.Look(20.0f, 10000.0f);
    for(int column = 0; column < 4; ++column) {
        for(int row = 0; row < 4; ++row) {
            Check(std::isfinite(camera.GetView()[column][row]),
                  "Pitch clamp produced an invalid view");
        }
    }
}

void TestBounds() {
    ModelDescription model;
    model.position = {4, 5, 6};
    model.rotation = {0, 0, 90};
    model.scale = {-2, 3, 4};
    const auto bounds = TransformBounds({{-1, -2, -3}, {1, 2, 3}}, MakeModelMatrix(model));
    Check(Near(bounds.minimum.x, -2) && Near(bounds.minimum.y, 3) && Near(bounds.minimum.z, -6),
          "Rotated/mirrored bounds minimum is incorrect");
    Check(Near(bounds.maximum.x, 10) && Near(bounds.maximum.y, 7) && Near(bounds.maximum.z, 18),
          "Rotated/mirrored bounds maximum is incorrect");

    const Frustum identity(glm::mat4(1.0f));
    Check(identity.Intersects({{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}}),
          "Visible bounds were culled");
    Check(identity.Intersects({{1.0f, 0, 0}, {2, 1, 1}}), "Intersecting bounds were culled");
    Check(!identity.Intersects({{2, 0, 0}, {3, 1, 1}}), "Offscreen bounds were not culled");
    Check(!identity.Intersects({{0, 0, 2}, {1, 1, 3}}), "Far-plane bounds were not culled");

    Camera camera;
    camera.Resize(800, 600);
    const Frustum perspective(camera.GetProjection() * camera.GetView());
    Check(perspective.Intersects({{-1, -1, -1}, {1, 1, 1}}),
          "Perspective frustum rejected visible geometry");
    Check(!perspective.Intersects({{-1, -1, 10}, {1, 1, 12}}),
          "Geometry behind the camera was not culled");

    model.position.x = std::numeric_limits<float>::quiet_NaN();
    bool rejected = false;
    try {
        MakeModelMatrix(model);
    } catch(const std::invalid_argument&) {
        rejected = true;
    }
    Check(rejected, "Nonfinite transforms were accepted");

    const float largest = std::numeric_limits<float>::max();
    const float smallest = std::numeric_limits<float>::denorm_min();
    const Bounds point{glm::vec3(smallest), glm::vec3(smallest)};
    const auto tiny = TransformBounds(point, glm::mat4(1.0f));
    Check(tiny.minimum == point.minimum && tiny.maximum == point.maximum,
          "Bounds midpoint calculation underflowed a representable point");

    glm::mat4 cancellation(1.0f);
    cancellation[0][0] = 2.0f;
    cancellation[1][0] = -2.0f;
    const auto cancelled = TransformBounds({{largest, largest, 0}, {largest, largest, 0}}, cancellation);
    Check(cancelled.minimum == glm::vec3(0, largest, 0) && cancelled.maximum == cancelled.minimum,
          "Finite bounds were lost to overflowing intermediate products");
    CheckInvalid([&] { TransformBounds({{-largest, 0, 0}, {largest, 0, 0}}, cancellation); },
                 "Unrepresentable bounds were accepted");

    model = {};
    model.position = {4, 5, 6};
    model.scale = {0, -2, 0};
    const auto flat = TransformBounds({{-1, -1, -1}, {1, 1, 1}}, MakeModelMatrix(model));
    Check(flat.minimum == glm::vec3(4, 3, 6) && flat.maximum == glm::vec3(4, 7, 6),
          "Zero or negative scale bounds became illegal");
}

void TestNormalMatrices() {
    const glm::dvec3 normal = glm::normalize(glm::dvec3(1, 2, 3));
    const glm::dvec3 tangent(2, -1, 0);
    const auto bitangent = glm::cross(normal, tangent);
    ModelDescription model;
    model.rotation = {23, -47, 61};
    const std::array<glm::vec3, 4> scales = {
        glm::vec3(2, 3, 0.25f), glm::vec3(-2, 3, 0.25f),
        glm::vec3(-2, -3, 0.25f), glm::vec3(-2, -3, -0.25f)};
    for(const auto& scale : scales) {
        model.scale = scale;
        const auto matrix = MakeModelMatrix(model);
        const auto normals = MakeNormalMatrix(matrix);
        const glm::dmat3 linear(matrix);
        const auto transformedNormal = glm::normalize(glm::dmat3(normals) * normal);
        Check(std::abs(glm::dot(transformedNormal, glm::normalize(linear * tangent))) < 1e-6 &&
                  std::abs(glm::dot(transformedNormal, glm::normalize(linear * bitangent))) < 1e-6,
              "Inverse-transpose normals are not perpendicular to transformed tangents");
        const auto product = glm::transpose(linear) * glm::dmat3(normals);
        for(int column = 0; column < 3; ++column) {
            for(int row = 0; row < 3; ++row) {
                Check(std::abs(product[column][row] - (column == row ? 1.0 : 0.0)) < 1e-6,
                      "Ordinary normal matrix is not the inverse transpose");
            }
        }
        model.position = {std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), 7};
        Check(MakeNormalMatrix(MakeModelMatrix(model)) == normals,
              "Translation changed the normal matrix");
        model.position = glm::vec3(0.0f);
    }

    model = {};
    model.scale = {-2, 3, 4};
    const auto mirrored = MakeNormalMatrix(MakeModelMatrix(model));
    Check(Near(mirrored[0][0], -0.5f) && Near(mirrored[1][1], 1.0f / 3.0f) &&
              Near(mirrored[2][2], 0.25f),
          "Mirroring incorrectly negated the entire normal transform");
    Check(MakeNormalMatrix(glm::mat4(1.0f)) == glm::mat3(1.0f), "Identity normal matrix changed");
    for(int mask = 0; mask < 8; ++mask) {
        const glm::vec3 scale((mask & 1) != 0 ? -2.0f : 2.0f,
                              (mask & 2) != 0 ? -3.0f : 3.0f,
                              (mask & 4) != 0 ? -4.0f : 4.0f);
        const int negativeCount = ((mask & 1) != 0) + ((mask & 2) != 0) + ((mask & 4) != 0);
        Check(IsMirrored(scale) == (negativeCount % 2 != 0), "Incorrect odd/even mirroring parity");
        for(int axis = 0; axis < 3; ++axis) {
            auto singular = scale;
            singular[axis] = -0.0f;
            Check(!IsMirrored(singular), "Singular TRS was classified as mirrored");
            model.scale = singular;
            model.rotation = {23, -47, 61};
            CheckZeroNormalMatrix(MakeNormalMatrix(MakeModelMatrix(model)));
        }
    }
    CheckZeroNormalMatrix(MakeNormalMatrix(glm::mat4(0.0f)));
    glm::mat4 dependent(1.0f);
    dependent[0] = {1, 2, 3, 0};
    dependent[1] = dependent[0] * 2.0f;
    dependent[2] = {4, 5, 6, 0};
    CheckZeroNormalMatrix(MakeNormalMatrix(dependent));
    dependent[0] = {354361, 678761, 494949, 0};
    dependent[1] = {1276093, 385693, 117824, 0};
    dependent[2] = dependent[0] * 3.0f + dependent[1] * 7.0f;
    CheckZeroNormalMatrix(MakeNormalMatrix(dependent));

    glm::mat4 shear(1.0f);
    shear[1][0] = 0.5f;
    shear[2][1] = -0.25f;
    const auto shearProduct = glm::transpose(glm::dmat3(shear)) * glm::dmat3(MakeNormalMatrix(shear));
    Check(shearProduct == glm::dmat3(1.0), "Sheared normal transform is not the inverse transpose");
    glm::mat4 nearlySingular(1.0f);
    nearlySingular[0][1] = 1.0f;
    nearlySingular[1][0] = 1.0f;
    nearlySingular[1][1] = std::nextafter(1.0f, 2.0f);
    Check(glm::determinant(glm::dmat3(MakeNormalMatrix(nearlySingular))) != 0.0,
          "Small nonzero determinant was treated as singular");

    const float largest = std::numeric_limits<float>::max();
    const float smallest = std::numeric_limits<float>::denorm_min();
    const std::array<glm::vec3, 7> extremes = {
        glm::vec3(largest), glm::vec3(std::numeric_limits<float>::min()), glm::vec3(smallest),
        glm::vec3(largest, 1, 1), glm::vec3(smallest, 1, 1),
        glm::vec3(1e30f, -1e-30f, 1), glm::vec3(-largest, largest / 2.0f, largest / 4.0f)};
    for(const auto& scale : extremes) {
        model = {};
        model.scale = scale;
        const auto matrix = MakeModelMatrix(model);
        const auto normals = MakeNormalMatrix(matrix);
        const double factor = static_cast<double>(scale.x) * normals[0][0];
        Check(factor > 0.0 && std::isfinite(factor), "Extreme normals lost their positive common scale");
        for(int column = 0; column < 3; ++column) {
            Check(std::abs(static_cast<double>(scale[column]) * normals[column][column] / factor - 1.0) < 1e-6,
                  "Extreme inverse transpose lost a useful axis or changed relative scales");
            for(int row = 0; row < 3; ++row) {
                Check(std::isfinite(normals[column][row]) &&
                          std::fpclassify(normals[column][row]) != FP_SUBNORMAL,
                      "Extreme normal transform contains nonfinite or subnormal values");
            }
        }
        Check(IsFinite(normals * glm::vec3(normal)), "Extreme normal/vector product overflowed");
        const auto diagonalNormal = normals * glm::normalize(glm::vec3(0, 1, 1));
        Check(std::fpclassify(diagonalNormal.y) != FP_SUBNORMAL &&
                  std::fpclassify(diagonalNormal.z) != FP_SUBNORMAL,
              "Extreme normal/vector product underflowed without multiplication headroom");
    }
    Check(IsMirrored({-largest, largest, smallest}) && IsMirrored({-smallest, smallest, smallest}) &&
              !IsMirrored({-largest, -smallest, largest}),
          "Mirroring depended on an overflowing or underflowing determinant");
    model.scale = {largest, smallest, 1};
    CheckZeroNormalMatrix(MakeNormalMatrix(MakeModelMatrix(model)));

    model.position = {largest, -largest, largest};
    model.rotation = {largest, -largest, largest};
    model.scale = {largest, -largest, largest};
    const auto extremeModel = MakeModelMatrix(model);
    for(int column = 0; column < 4; ++column) {
        for(int row = 0; row < 4; ++row) {
            Check(std::isfinite(extremeModel[column][row]), "Extreme finite TRS produced a nonfinite matrix");
        }
    }
    const auto extremeNormals = MakeNormalMatrix(extremeModel);
    Check(glm::determinant(glm::dmat3(extremeNormals)) != 0.0,
          "Rotated large uniform scales lost their normal transform");
    Check(IsFinite(extremeNormals * glm::vec3(normal)), "Rotated extreme normal product is nonfinite");

    for(const float bad : {std::numeric_limits<float>::infinity(),
                           -std::numeric_limits<float>::infinity(),
                           std::numeric_limits<float>::quiet_NaN()}) {
        for(int axis = 0; axis < 3; ++axis) {
            glm::vec3 scale(-1, 1, 1);
            scale[axis] = bad;
            Check(!IsMirrored(scale), "Nonfinite TRS was classified as mirrored");
            for(int column = 0; column < 3; ++column) {
                glm::mat4 invalid(1.0f);
                invalid[column][axis] = bad;
                CheckZeroNormalMatrix(MakeNormalMatrix(invalid));
            }
            model = {};
            model.scale[axis] = bad;
            CheckInvalid([&] { MakeModelMatrix(model); }, "Nonfinite scale was accepted");
            model = {};
            model.rotation[axis] = bad;
            CheckInvalid([&] { MakeModelMatrix(model); }, "Nonfinite rotation was accepted");
        }
    }
}

void TestLighting() {
    const SceneLighting defaults;
    Check(defaults.enabled && defaults.color == glm::vec3(1.0f) &&
              defaults.intensity == 0.8f && defaults.ambientStrength == 0.2f,
          "Lighting defaults changed");
    const double component = -1.0 / std::sqrt(3.0);
    for(int axis = 0; axis < 3; ++axis) {
        Check(std::abs(static_cast<double>(defaults.direction[axis]) - component) < 1e-7,
              "Default sun direction is not normalized (-1, -1, -1)");
    }
    Check(ValidateSceneLighting(defaults) == defaults, "Validating defaults changed stored lighting");
    const SceneDocument empty;
    Check(empty.models.empty() && empty.lighting == defaults, "Empty document lost default-enabled lighting");

    auto lighting = defaults;
    lighting.enabled = false;
    lighting.direction = {3, -4, 0};
    lighting.color = {0.25f, 0.5f, 0.75f};
    lighting.intensity = 2.5f;
    lighting.ambientStrength = 0.75f;
    const auto normalized = ValidateSceneLighting(lighting);
    Check(Near(normalized.direction.x, 0.6f) && Near(normalized.direction.y, -0.8f) &&
              normalized.direction.z == 0.0f && lighting.direction == glm::vec3(3, -4, 0),
          "Lighting validation did not normalize a copy");
    Check(!normalized.enabled && normalized.color == lighting.color &&
              normalized.intensity == lighting.intensity && normalized.ambientStrength == lighting.ambientStrength,
          "Normalization changed other lighting fields");
    Check(ValidateSceneLighting(normalized) == normalized, "Normalized stored lighting did not compare equal");
    lighting.direction = {-0.6190559f, -0.823850751f, -0.694954455f};
    const auto stable = ValidateSceneLighting(lighting);
    for(int iteration = 0; iteration < 16; ++iteration) {
        Check(ValidateSceneLighting(stable) == stable, "Repeated normalization drifted stored float direction");
    }

    const float largest = std::numeric_limits<float>::max();
    const float smallest = std::numeric_limits<float>::denorm_min();
    const std::array<glm::vec3, 5> directions = {
        glm::vec3(largest, -largest, largest), glm::vec3(smallest, -smallest, smallest),
        glm::vec3(largest, smallest, 0), glm::vec3(std::numeric_limits<float>::min(), 0, 0),
        glm::vec3(0, 0, -smallest)};
    for(const auto& direction : directions) {
        lighting.direction = direction;
        const auto result = ValidateSceneLighting(lighting);
        Check(IsFinite(result.direction) && std::abs(glm::length(glm::dvec3(result.direction)) - 1.0) < 1e-7,
              "Extreme finite sun direction did not normalize safely");
        Check(glm::dot(glm::dvec3(result.direction), glm::dvec3(direction)) > 0.0,
              "Normalization reversed the sun direction");
    }
    lighting.direction = {0, -0.0f, 0};
    CheckInvalid([&] { ValidateSceneLighting(lighting); }, "Zero sun direction was accepted while disabled");

    for(const float bad : {std::numeric_limits<float>::infinity(),
                           -std::numeric_limits<float>::infinity(),
                           std::numeric_limits<float>::quiet_NaN()}) {
        for(int axis = 0; axis < 3; ++axis) {
            lighting = defaults;
            lighting.direction[axis] = bad;
            CheckInvalid([&] { ValidateSceneLighting(lighting); }, "Nonfinite sun direction was accepted");
            lighting = defaults;
            lighting.color[axis] = bad;
            CheckInvalid([&] { ValidateSceneLighting(lighting); }, "Nonfinite sun color was accepted");
        }
        lighting = defaults;
        lighting.intensity = bad;
        CheckInvalid([&] { ValidateSceneLighting(lighting); }, "Nonfinite sun intensity was accepted");
        lighting = defaults;
        lighting.ambientStrength = bad;
        CheckInvalid([&] { ValidateSceneLighting(lighting); }, "Nonfinite ambient strength was accepted");
    }
    for(const float bad : {-smallest, std::nextafter(1.0f, 2.0f)}) {
        for(int axis = 0; axis < 3; ++axis) {
            lighting = defaults;
            lighting.color[axis] = bad;
            CheckInvalid([&] { ValidateSceneLighting(lighting); }, "Out-of-range sun color was accepted");
        }
        lighting = defaults;
        lighting.ambientStrength = bad;
        CheckInvalid([&] { ValidateSceneLighting(lighting); }, "Out-of-range ambient strength was accepted");
    }
    for(const float bad : {-smallest, std::nextafter(4.0f, 5.0f)}) {
        lighting = defaults;
        lighting.intensity = bad;
        CheckInvalid([&] { ValidateSceneLighting(lighting); }, "Out-of-range sun intensity was accepted");
    }
    for(const float endpoint : {0.0f, 1.0f}) {
        lighting = defaults;
        lighting.color = glm::vec3(endpoint);
        lighting.intensity = endpoint * 4.0f;
        lighting.ambientStrength = endpoint;
        Check(ValidateSceneLighting(lighting) == lighting, "Inclusive lighting range boundary was rejected");
    }
}

void TestLayout() {
    VertexBufferLayout layout;
    layout.Push<float>(4, 3);
    layout.Push<unsigned>(1, 1);
    Check(layout.GetStride() == 16, "Incorrect vertex layout stride");
    Check(layout.GetElements()[0].location == 4 && layout.GetElements()[1].location == 1 &&
              layout.GetElements()[1].offset == 12,
          "Attribute locations depend on insertion order");
    bool rejected = false;
    try {
        VertexBufferElement::GetSizeOfType(GL_DOUBLE);
    } catch(const std::invalid_argument&) {
        rejected = true;
    }
    Check(rejected, "Unknown attribute type silently returned a zero size");
    rejected = false;
    try {
        layout.Push<float>(4, 3);
    } catch(const std::invalid_argument&) {
        rejected = true;
    }
    Check(rejected, "Duplicate attribute location was accepted");
}
} // namespace

int main() {
    try {
        TestCamera();
        TestBounds();
        TestNormalMatrices();
        TestLighting();
        TestLayout();
        std::cout << "Scene, lighting, camera, layout, and ownership tests passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
