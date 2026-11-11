#pragma once
#include <cstdio>
#include <cstdlib>
#include <format>
#include <string>
#include <source_location>
#include <string_view>

[[noreturn]] inline void
fatal(std::string_view msg, std::source_location loc = std::source_location::current()) {
    const std::string out =
        std::format("Fatal error: {} at {}:{}\n", msg, loc.file_name(), loc.line());

    std::fwrite(out.data(), 1, out.size(), stderr);

    std::abort();
}
