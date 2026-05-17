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
        return std::put_time(std::localtime(&t_c), "%F %T");
    }

    // Для имени файла возьмите дату с форматом "%Y_%m_%d"
    std::string GetFileTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&t_c);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y_%m_%d");
        return oss.str();
    }
    
    // Получить имя файла на основе текущей даты
    std::string GetLogFileName() const {
        return "/var/log/sample_log_" + GetFileTimeStamp() + ".log";
    }
    
    // Проверить и при необходимости сменить файл лога
    void CheckAndRotateFile() {
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
        static Logger obj;
        return obj;
    }

    // Выведите в поток все аргументы.
    template<typename T>
    void LogImpl(std::ostream& os, const T& arg) {
        os << arg;
    }
    
    template<typename T, typename... Ts>
    void LogImpl(std::ostream& os, const T& arg, const Ts&... args) {
        os << arg;
        LogImpl(os, args...);
    }

    template<class... Ts>
    void Log(const Ts&... args) {
        std::lock_guard<std::mutex> lock(m_);
        
        // Проверяем необходимость смены файла
        CheckAndRotateFile();
        
        // Выводим временную метку
        log_file_ << GetTimeStamp() << ": "sv;
        
        // Выводим все аргументы
        LogImpl(log_file_, args...);
        
        // Завершаем строку
        log_file_ << std::endl;
    }

    // Установите manual_ts_. Учтите, что эта операция может выполняться
    // параллельно с выводом в поток, вам нужно предусмотреть 
    // синхронизацию.
    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard<std::mutex> lock(m_);
        manual_ts_ = ts;
        
        // При смене времени может измениться дата, 
        // поэтому при следующем логировании проверим файл
        // (не закрываем здесь, т.к. это может быть опасно)
    }

private:
    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    mutable std::ofstream log_file_;
    std::string current_filename_;
    mutable std::mutex m_;
};