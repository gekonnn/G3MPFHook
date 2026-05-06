#include "pch.h"

Logger* Logger::instancePtr = nullptr;

Logger::Logger()
{
    fopen_s(&m_logFile, "latest.log", "w+");
    //m_logFile = nullptr;
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

    std::string lv;

    switch (level)
    {
    case LOG_NONE:		break;
    case LOG_INFO:		lv = "INFO"; break;
    case LOG_WARNING:	lv = "WARN"; break;
    case LOG_ERROR:		lv = "ERRO"; break;
    }

    std::string s_prefix_c = "[" +  lv + "] ";
    std::string s_fmt_c = (lv.empty() ? "" : s_prefix_c) + fmt + '\n';
    vprintf_s(s_fmt_c.c_str(), args);

    if (m_logFile)
    {
        std::time_t t = std::time(nullptr);
        std::tm localTime;
        localtime_s(&localTime, &t);

        std::ostringstream oss;
        oss << std::put_time(&localTime, "%d.%m.%Y %H:%M:%S");
        std::string s_timestamp = oss.str();

        std::string lv;
        switch (level)
        {
        case LOG_INFO:		lv = "INFO"; break;
        case LOG_WARNING:	lv = "WARN"; break;
        case LOG_ERROR:		lv = "ERRO"; break;
        default:            lv = ""; break;
        }

        std::string s_prefix = lv.empty() ? "" : " [ " + lv + " ] ";
        fprintf(m_logFile, "%s%s %s\n", s_timestamp.c_str(), s_prefix.c_str(), buf);
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
