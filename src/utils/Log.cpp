#include "utils/Log.h"

#include <iostream>
#include <mutex>

void Log(LogLevel level, std::string_view message) noexcept {
#ifdef NDEBUG
    if(level == LogLevel::Debug) {
        return;
    }
#endif
    try {
        static std::mutex sinkMutex;
        constexpr std::string_view labels[] = {"debug", "info", "warning", "error"};
        const std::lock_guard lock(sinkMutex);
        std::clog << '[' << labels[static_cast<std::size_t>(level)] << "] " << message << '\n';
    } catch(...) {
        // Logging must not throw through a GLFW/GL callback or a destructor.
    }
}
