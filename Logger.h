#pragma once
#include <fstream>
#include <string>
#include <cstdarg>
#include <mutex>

class Logger
{
public:
    static Logger& Get()
    {
        static Logger instance;
        return instance;
    }

    void Init(const std::string& filename)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open())
            m_file.close();
        m_file.open(filename, std::ios::out | std::ios::trunc);
        if (m_file.is_open())
        {
            m_file << "=== ImageViewer Log Started ===" << std::endl;
            m_file.flush();
        }
    }

    void Log(const char* format, ...)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_file.is_open())
            return;

        char buffer[2048];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        m_file << buffer << std::endl;
        m_file.flush();
    }

    void LogError(const char* format, ...)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_file.is_open())
            return;

        char buffer[2048];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        m_file << "[ERROR] " << buffer << std::endl;
        m_file.flush();
    }

    void Close()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open())
        {
            m_file << "=== Log Closed ===" << std::endl;
            m_file.close();
        }
    }

    ~Logger()
    {
        Close();
    }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::ofstream m_file;
    std::mutex m_mutex;
};

#define LOG(fmt, ...) Logger::Get().Log(fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::Get().LogError(fmt, ##__VA_ARGS__)
