#pragma once

#include <iostream>
#include <string>
#include <boost/json.hpp>

namespace logger {

inline void LogError(int code, const std::string& message, const std::string& context) {
    std::cerr << "[ERROR] " << context << ": " << message << " (code: " << code << ")" << std::endl;
}

inline void LogDebug(const std::string& message) {
    std::cout << "[DEBUG] " << message << std::endl;
}

inline void LogRequestReceived(const boost::json::object& data) {
    // Логирование запроса
    std::cout << "[REQUEST] ";
    for (const auto& [key, value] : data) {
        std::cout << key << "=" << value << " ";
    }
    std::cout << std::endl;
}

inline void LogResponseSent(const boost::json::object& data) {
    // Логирование ответа
    std::cout << "[RESPONSE] ";
    for (const auto& [key, value] : data) {
        std::cout << key << "=" << value << " ";
    }
    std::cout << std::endl;
}

} // namespace logger