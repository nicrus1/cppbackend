#pragma once

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>
#include <ctime>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    std::chrono::system_clock::time_point GetTime() const {
        std::lock_guard<std::mutex> lock(m_);
        if (manual_ts_) {
            return *manual_ts_;
        }
        return std::chrono::system_clock::now();
    }

    std::string GetTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &t_c);
#else
        localtime_r(&t_c, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%F %T");
        return oss.str();
    }
    
    std::string GetLogFileName() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &t_c);
#else
        localtime_r(&t_c, &tm);
#endif
        std::ostringstream oss;
        oss << "log_" << std::put_time(&tm, "%Y_%m_%d") << ".txt";
        return oss.str();
    }
    
    void RotateFileIfNeeded() {
        std::string new_filename = GetLogFileName();
        if (!log_file_.is_open() || current_filename_ != new_filename) {
            if (log_file_.is_open()) {
                log_file_.close();
            }
            current_filename_ = new_filename;
            log_file_.open(current_filename_, std::ios::app);
        }
    }

    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

public:
    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }

    // Базовый случай рекурсии
    void LogImpl(std::ostream& os) const {}

    // Рекурсивный вывод аргументов
    template<typename T, typename... Ts>
    void LogImpl(std::ostream& os, const T& arg, const Ts&... args) const {
        os << arg;
        LogImpl(os, args...);
    }

    template<class... Ts>
    void Log(const Ts&... args) {
        std::lock_guard<std::mutex> lock(m_);
        RotateFileIfNeeded();
        log_file_ << GetTimeStamp() << ": ";
        LogImpl(log_file_, args...);
        log_file_ << std::endl;
    }

    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard<std::mutex> lock(m_);
        manual_ts_ = ts;
    }

private:
    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    std::ofstream log_file_;
    std::string current_filename_;
    mutable std::mutex m_;
};