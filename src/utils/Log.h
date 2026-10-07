#pragma once

#include <string_view>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

// Whether the sink would emit this level at all. Callers on hot or diagnostic paths use this to
// avoid constructing messages (and taking the logger lock) when the level is discarded.
bool ShouldLog(LogLevel level) noexcept;

// Cold-path logging only. Each complete message is serialized across worker threads.
void Log(LogLevel level, std::string_view message) noexcept;
