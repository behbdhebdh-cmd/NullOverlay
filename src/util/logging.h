#pragma once

#include "pch.h"

namespace util {

enum class LogLevel {
    Info,
    Warning,
    Error,
};

void initializeLogging();
void shutdownLogging();
void log(LogLevel level, std::string_view message);
void logInfo(std::string_view message);
void logWarning(std::string_view message);
void logError(std::string_view message);

} // namespace util
