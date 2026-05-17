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
    auto GetTime() const {
        std::lock_guard<std::mutex> lock(m_);
        if (manual_ts_) {
            return *manual_ts_;
        }
        return std::chrono::system_clock::now();
    }

    auto GetTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_r(&t_c, &tm);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%F %T");
        return oss.str();
    }

    std::string GetFileTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_r(&t_c, &tm);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y_%m_%d");
        return oss.str();
    }
    
    std::string GetLogFileName() const {
        return "/var/log/sample_log_" + GetFileTimeStamp() + ".log";
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
    void LogImpl(std::ostream& os) const {
        // ничего не делаем
    }

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
        
        // Временная метка с двоеточием и пробелом после неё
        log_file_ << GetTimeStamp() << ": ";
        
        // Вывод всех аргументов
        LogImpl(log_file_, args...);
        
        // Завершение строки
        log_file_ << std::endl;
    }

    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard<std::mutex> lock(m_);
        manual_ts_ = ts;
        // При смене даты файл будет ротирован при следующем логировании
    }

private:
    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    mutable std::ofstream log_file_;
    std::string current_filename_;
    mutable std::mutex m_;
};