#include "pch.h"

Logger* Logger::instancePtr = nullptr;

Logger::Logger()
{
#ifdef OS_WINDOWS
    DWORD consoleMode;
    HANDLE outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(outputHandle, &consoleMode))
        SetConsoleMode(outputHandle, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

    fopen_s(&m_logFile, "latest.log", "w+");
}

Logger::~Logger()
{
    if (m_logFile)
        fclose(m_logFile);
}

Logger* Logger::GetInstance()
{
    if (instancePtr == nullptr)
        instancePtr = new Logger();
    return instancePtr;
}

void Logger::LogV(LogLevel level, bool send_ipc, const char* fmt, va_list args)
{
    if (!m_loggerEnabled) return;
    if (level > 0 && level < m_minLogLevel) return;

    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, args);

    if (send_ipc)
    {
        size_t len = strlen(buf);
        char* copy = (char*)malloc(len + 1);

        if (copy)
        {
            memcpy(copy, buf, len + 1);
            zmq_msg_struct msg{ level, (uint32_t)len, copy };
            G3MPFHook::GetInstance()->zmq_send_message(msg);
            zms_free(&msg);
        }
    }

    const char* lv = "";
    unsigned int color = 37;

    switch (level)
    {
    case LOG_INFO:      lv = "INFO"; color = 36; break;
    case LOG_WARNING:   lv = "WARN"; color = 93; break;
    case LOG_ERROR:     lv = "ERRO"; color = 31; break;
    default:
        break;
    }

    if (*lv)
        printf("[ \x1b[%um%s\x1b[0m ] %s\n", color, lv, buf);
    else
        printf("%s\n", buf);

    if (m_logFile)
    {
        std::time_t t = std::time(nullptr);
        std::tm localTime;
        localtime_s(&localTime, &t);

        std::ostringstream oss;
        oss << std::put_time(&localTime, "%d.%m.%Y %H:%M:%S");

        if (*lv)
            fprintf(m_logFile, "%s [ %s ] %s\n",
                oss.str().c_str(), lv, buf);
        else
            fprintf(m_logFile, "%s %s\n",
                oss.str().c_str(), buf);

        fflush(m_logFile);
    }
}

void Logger::Log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Logger::GetInstance()->LogV(LOG_NONE, false, fmt, args);
    va_end(args);
}

void Logger::Log(LogLevel level, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Logger::GetInstance()->LogV(level, false, fmt, args);
    va_end(args);
}

void Logger::LogIPC(LogLevel level, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Logger::GetInstance()->LogV(level, true, fmt, args);
    va_end(args);
}

void Logger::SetEnabled(bool v)
{
    m_loggerEnabled = v;
}

bool Logger::GetEnabled() const
{
    return m_loggerEnabled;
}
