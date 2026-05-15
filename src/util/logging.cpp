#include "util/logging.h"

namespace {

std::mutex g_logMutex;
std::ofstream g_logFile;
HANDLE g_consoleOutput = nullptr;
bool g_consoleAllocated = false;

WORD consoleColor(util::LogLevel level) {
    switch (level) {
    case util::LogLevel::Info:
        return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    case util::LogLevel::Warning:
        return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    case util::LogLevel::Error:
        return FOREGROUND_RED | FOREGROUND_INTENSITY;
    }
    return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
}

const char* levelName(util::LogLevel level) {
    switch (level) {
    case util::LogLevel::Info:
        return "INFO";
    case util::LogLevel::Warning:
        return "WARN";
    case util::LogLevel::Error:
        return "ERROR";
    }
    return "INFO";
}

std::string timestamp() {
    SYSTEMTIME time{};
    GetLocalTime(&time);

    char buffer[64]{};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%04u-%02u-%02u %02u:%02u:%02u.%03u",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds);
    return buffer;
}

std::filesystem::path logPath() {
    wchar_t tempPath[MAX_PATH]{};
    const DWORD length = GetTempPathW(static_cast<DWORD>(std::size(tempPath)), tempPath);
    if (length == 0 || length >= std::size(tempPath)) {
        return std::filesystem::path(L"null_overlay.log");
    }
    return std::filesystem::path(tempPath) / L"null_overlay.log";
}

void initializeConsole() {
    if (g_consoleAllocated) {
        return;
    }

    if (!AllocConsole() && GetLastError() != ERROR_ACCESS_DENIED) {
        return;
    }

    g_consoleAllocated = true;
    SetConsoleTitleW(L"NullOverlay Debug Console");
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    FILE* ignored = nullptr;
    freopen_s(&ignored, "CONOUT$", "w", stdout);
    freopen_s(&ignored, "CONOUT$", "w", stderr);
    freopen_s(&ignored, "CONIN$", "r", stdin);

    g_consoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_consoleOutput != nullptr && g_consoleOutput != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(g_consoleOutput, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        const char* banner =
            "\n"
            "============================================================\n"
            " NullOverlay live debug console\n"
            " Every important hook/render/JNI event is mirrored here.\n"
            "============================================================\n";
        DWORD written = 0;
        WriteConsoleA(g_consoleOutput, banner, static_cast<DWORD>(std::strlen(banner)), &written, nullptr);
        SetConsoleTextAttribute(g_consoleOutput, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }
}

} // namespace

namespace util {

void initializeLogging() {
    bool opened = false;
    {
        std::lock_guard lock(g_logMutex);
        if (g_logFile.is_open()) {
            return;
        }

        initializeConsole();

        g_logFile.open(logPath(), std::ios::out | std::ios::app);
        if (g_logFile.is_open()) {
            g_logFile << "\n=== NullOverlay session start ===\n";
            g_logFile.flush();
            opened = true;
        }
    }

    std::ostringstream message;
    message << "Logging initialized. Console=" << (g_consoleAllocated ? "yes" : "no")
            << ", file=" << (opened ? "yes" : "no")
            << ", file path=" << logPath().string();
    logInfo(message.str());
}

void shutdownLogging() {
    std::lock_guard lock(g_logMutex);
    if (g_consoleOutput != nullptr && g_consoleOutput != INVALID_HANDLE_VALUE) {
        const char* footer = "=== NullOverlay session end ===\n";
        DWORD written = 0;
        WriteConsoleA(g_consoleOutput, footer, static_cast<DWORD>(std::strlen(footer)), &written, nullptr);
    }

    if (g_logFile.is_open()) {
        g_logFile << "=== NullOverlay session end ===\n";
        g_logFile.flush();
        g_logFile.close();
    }
}

void log(LogLevel level, std::string_view message) {
    std::lock_guard lock(g_logMutex);

    std::ostringstream line;
    line << "[" << timestamp() << "] [" << levelName(level) << "] " << message << "\n";

    const std::string output = line.str();
    OutputDebugStringA(output.c_str());

    if (g_consoleOutput != nullptr && g_consoleOutput != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(g_consoleOutput, consoleColor(level));
        DWORD written = 0;
        WriteConsoleA(g_consoleOutput, output.c_str(), static_cast<DWORD>(output.size()), &written, nullptr);
        SetConsoleTextAttribute(g_consoleOutput, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }

    if (g_logFile.is_open()) {
        g_logFile << output;
        g_logFile.flush();
    }
}

void logInfo(std::string_view message) {
    log(LogLevel::Info, message);
}

void logWarning(std::string_view message) {
    log(LogLevel::Warning, message);
}

void logError(std::string_view message) {
    log(LogLevel::Error, message);
}

} // namespace util
