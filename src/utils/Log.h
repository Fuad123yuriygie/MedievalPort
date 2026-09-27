#pragma once

#include <string_view>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

// Cold-path logging only. Each complete message is serialized across worker threads.
void Log(LogLevel level, std::string_view message) noexcept;
