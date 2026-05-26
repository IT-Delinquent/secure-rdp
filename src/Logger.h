#pragma once

#include <string>

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
public:
    static void Init();
    static void Shutdown();

    static void Debug(const std::wstring& message);
    static void Info(const std::wstring& message);
    static void Warn(const std::wstring& message);
    static void Error(const std::wstring& message);

    static std::wstring GetLogFilePath();

private:
    static void Write(LogLevel level, const std::wstring& message);
    static const wchar_t* LevelTag(LogLevel level);
};

#define LOG_INFO(msg) Logger::Info(msg)
#define LOG_WARN(msg) Logger::Warn(msg)
#define LOG_ERROR(msg) Logger::Error(msg)
#define LOG_DEBUG(msg) Logger::Debug(msg)
