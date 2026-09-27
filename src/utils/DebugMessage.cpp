#include "utils/DebugMessage.h"

#include "graphics/GraphicsContext.h"
#include "utils/Log.h"

#include <glad/glad.h>
#include <string>

namespace
{
void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                            const GLchar* message, const void*) noexcept {
    try {
        const auto level = severity == GL_DEBUG_SEVERITY_HIGH     ? LogLevel::Error
                           : severity == GL_DEBUG_SEVERITY_MEDIUM ? LogLevel::Warning
                                                                  : LogLevel::Debug;
        Log(level,
            "OpenGL id=" + std::to_string(id) + " source=" + std::to_string(source) +
                " type=" + std::to_string(type) + " severity=" + std::to_string(severity) + ": " +
                std::string(message, static_cast<std::size_t>(length)));
    } catch(...) {
        Log(LogLevel::Error, "OpenGL diagnostic (message unavailable)");
    }
}
} // namespace

void InstallDebugOutput(const GraphicsContext& context) {
    context.RequireCurrent();
    // Debug output is core since 4.3; this GLAD build has no extension flags.
    if(!GLAD_GL_VERSION_4_3 || !glDebugMessageCallback) {
        Log(LogLevel::Warning, "OpenGL debug output is unavailable");
        return;
    }
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(DebugCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE,
                          GL_DONT_CARE,
                          GL_DEBUG_SEVERITY_NOTIFICATION,
                          0,
                          nullptr,
                          GL_FALSE);
}
