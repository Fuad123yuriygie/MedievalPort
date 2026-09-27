#include "core/Application.h"
#include "core/Camera.h"
#include "core/SceneMath.h"
#include "graphics/IndexBuffer.h"
#include "graphics/Shader.h"
#include "graphics/TextureArray.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"
#include "graphics/VertexBufferLayout.h"

#include <cmath>
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
        TestLayout();
        std::cout << "Scene, camera, layout, and ownership tests passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
