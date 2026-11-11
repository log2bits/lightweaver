#pragma once

#include <cstdio>
#include <format>
#include <string>
#include <string_view>

inline void logWrite(std::string_view level, std::string_view msg) {
    const std::string out = std::format("[{}] {}\n", level, msg);

    std::fwrite(out.data(), 1, out.size(), stderr);
}

inline void logInfo(std::string_view msg) {
    logWrite("info", msg);
}

inline void logWarn(std::string_view msg) {
    logWrite("warn", msg);
}

inline void logError(std::string_view msg) {
    logWrite("error", msg);
}
