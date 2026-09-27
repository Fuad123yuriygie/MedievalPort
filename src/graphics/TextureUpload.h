#pragma once

#include <array>
#include <glad/glad.h>

// Client-memory RGBA uploads must not inherit a PBO or pixel-store offsets from another pass.
class PixelUnpackScope {
public:
    PixelUnpackScope() {
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        for(std::size_t index = 0; index < parameters.size(); ++index) {
            glGetIntegerv(parameters[index], &values[index]);
            glPixelStorei(parameters[index], parameters[index] == GL_UNPACK_ALIGNMENT ? 1 : 0);
        }
    }

    ~PixelUnpackScope() {
        for(std::size_t index = 0; index < parameters.size(); ++index) {
            glPixelStorei(parameters[index], values[index]);
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(buffer));
    }

    PixelUnpackScope(const PixelUnpackScope&) = delete;
    PixelUnpackScope& operator=(const PixelUnpackScope&) = delete;

private:
    static constexpr std::array<GLenum, 6> parameters{GL_UNPACK_ALIGNMENT,
                                                      GL_UNPACK_ROW_LENGTH,
                                                      GL_UNPACK_IMAGE_HEIGHT,
                                                      GL_UNPACK_SKIP_PIXELS,
                                                      GL_UNPACK_SKIP_ROWS,
                                                      GL_UNPACK_SKIP_IMAGES};
    std::array<GLint, parameters.size()> values{};
    GLint buffer = 0;
};
