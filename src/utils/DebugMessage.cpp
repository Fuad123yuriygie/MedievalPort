#include "utils/DebugMessage.h"

#include "graphics/GraphicsContext.h"
#include "utils/Log.h"

#include <glad/glad.h>
#include <string>

namespace
{
LogLevel LevelForSeverity(GLenum severity) {
    switch(severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        return LogLevel::Error;
    case GL_DEBUG_SEVERITY_MEDIUM:
        return LogLevel::Warning;
    default:
        return LogLevel::Debug;
    }
}

void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                            const GLchar* message, const void*) noexcept {
    const auto level = LevelForSeverity(severity);
    // Filter before formatting: discarded severities never build a string or touch the logger.
    if(!ShouldLog(level)) {
        return;
    }
    try {
        Log(level,
            "OpenGL id=" + std::to_string(id) + " source=" + std::to_string(source) +
                " type=" + std::to_string(type) + " severity=" + std::to_string(severity) + ": " +
                std::string(message, static_cast<std::size_t>(length)));
    } catch(...) {
        Log(LogLevel::Error, "OpenGL diagnostic (message unavailable)");
    }
}
} // namespace

void InstallDebugOutput(const GraphicsContext& context, bool synchronous) {
    context.RequireCurrent();
    // Debug output is core since 4.3; this GLAD build has no extension flags.
    if(!GLAD_GL_VERSION_4_3 || !glDebugMessageCallback) {
        Log(LogLevel::Warning, "OpenGL debug output is unavailable");
        return;
    }
    glEnable(GL_DEBUG_OUTPUT);
    if(synchronous) {
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    }
    else {
        glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    }
    glDebugMessageCallback(DebugCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE,
                          GL_DONT_CARE,
                          GL_DEBUG_SEVERITY_NOTIFICATION,
                          0,
                          nullptr,
                          GL_FALSE);
}
