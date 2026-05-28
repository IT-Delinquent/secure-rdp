#include "Logger.h"

#include "Util.h"

#include <windows.h>

#include <fstream>
#include <mutex>

namespace {

std::mutex g_mutex;
std::wstring g_logPath;
bool g_initialized = false;

}  // namespace

void Logger::Init() {
    std::lock_guard lock(g_mutex);
    if (g_initialized) {
        return;
    }
    g_logPath = Util::GetAppDataDir() + L"\\app.log";
    g_initialized = true;
    Write(LogLevel::Info, L"=== Tiny RDP started ===");
}

void Logger::Shutdown() {
    std::lock_guard lock(g_mutex);
    if (g_initialized) {
        Write(LogLevel::Info, L"=== Tiny RDP exiting ===");
        g_initialized = false;
    }
}

std::wstring Logger::GetLogFilePath() {
    return g_logPath;
}

const wchar_t* Logger::LevelTag(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return L"DEBUG";
        case LogLevel::Info:
            return L"INFO ";
        case LogLevel::Warn:
            return L"WARN ";
        case LogLevel::Error:
            return L"ERROR";
    }
    return L"?    ";
}

void Logger::Write(LogLevel level, const std::wstring& message) {
    SYSTEMTIME st{};
    GetLocalTime(&st);

    wchar_t prefix[128]{};
    swprintf_s(prefix, L"[%04u-%02u-%02u %02u:%02u:%02u] [%s] ", st.wYear, st.wMonth, st.wDay, st.wHour,
               st.wMinute, st.wSecond, LevelTag(level));

    const std::wstring line = std::wstring(prefix) + message + L"\r\n";
    OutputDebugStringW(line.c_str());

    if (g_logPath.empty()) {
        return;
    }

    std::ofstream out(g_logPath, std::ios::app);
    if (!out) {
        return;
    }
    out << Util::WideToUtf8(line);
}

void Logger::Debug(const std::wstring& message) {
    Write(LogLevel::Debug, message);
}

void Logger::Info(const std::wstring& message) {
    Write(LogLevel::Info, message);
}

void Logger::Warn(const std::wstring& message) {
    Write(LogLevel::Warn, message);
}

void Logger::Error(const std::wstring& message) {
    Write(LogLevel::Error, message);
}
