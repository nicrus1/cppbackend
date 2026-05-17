#include "logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <sstream>

namespace logger {

void InitLogging() {
    std::ios::sync_with_stdio(false);
}

static std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::tm tm;
    gmtime_r(&time_t, &tm);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << "." 
        << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void LogRequestReceived(const boost::json::object& data) {
    boost::json::value json_data(data);
    std::cout << boost::json::serialize(boost::json::object{
        {"timestamp", GetTimestamp()},
        {"data", json_data},
        {"message", "request received"}
    }) << std::endl;
}

void LogResponseSent(const boost::json::object& data) {
    boost::json::value json_data(data);
    std::cout << boost::json::serialize(boost::json::object{
        {"timestamp", GetTimestamp()},
        {"data", json_data},
        {"message", "response sent"}
    }) << std::endl;
}

void LogServerStarted(const std::string& address, unsigned short port) {
    boost::json::object data;
    data["address"] = address;
    data["port"] = port;
    boost::json::value json_data(data);
    std::cout << boost::json::serialize(boost::json::object{
        {"timestamp", GetTimestamp()},
        {"data", json_data},
        {"message", "server started"}
    }) << std::endl;
}

void LogServerExited(int code, const std::string& exception) {
    boost::json::object data;
    data["code"] = code;
    if (!exception.empty()) {
        data["exception"] = exception;
    }
    boost::json::value json_data(data);
    std::cout << boost::json::serialize(boost::json::object{
        {"timestamp", GetTimestamp()},
        {"data", json_data},
        {"message", "server exited"}
    }) << std::endl;
}

void LogError(int code, const std::string& text, const std::string& where) {
    boost::json::object data;
    data["code"] = code;
    data["text"] = text;
    data["where"] = where;
    boost::json::value json_data(data);
    std::cout << boost::json::serialize(boost::json::object{
        {"timestamp", GetTimestamp()},
        {"data", json_data},
        {"message", "error"}
    }) << std::endl;
}

}  // namespace logger