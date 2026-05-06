#ifndef LOGGER_H
#define LOGGER_H

#pragma once

#include <stdarg.h>
#include <cstdio>

enum LogLevel
{
    LOG_NONE = 0,
    LOG_INFO = 1,
    LOG_WARNING = 2,
    LOG_ERROR = 3
};

class Logger
{
public:
    ~Logger();

    Logger(const Logger& obj) = delete;
    Logger& operator=(const Logger& obj) = delete;

    static Logger* GetInstance();

    static void Log(const char* fmt, ...);
    static void Log(LogLevel level, const char* fmt, ...);
    static void LogIPC(LogLevel level, const char* fmt, ...);

    void SetEnabled(bool v);
    bool GetEnabled() const;

    int& GetMinLogLevel() { return m_minLogLevel; }
    void SetMinLogLevel(const int& value) { m_minLogLevel = value; }
private:
    Logger();
    static Logger* instancePtr;

    FILE* m_logFile = nullptr;
    bool m_loggerEnabled = true;

    int m_minLogLevel = -1;
    void LogV(LogLevel level, bool send_ipc, const char* fmt, va_list args);
};

#endif
