#pragma once

#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/json/value.hpp>
#include <boost/json/serialize.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace logging {

namespace json = boost::json;
namespace logging = boost::log;

// Ключевые слова для атрибутов
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

// Инициализация логгера
void InitLogger();

// Форматирование времени в ISO формат
std::string GetCurrentTimestamp();

// Создание JSON-сообщения для лога
template<typename... Args>
void Log(boost::log::trivial::severity_level level, 
         std::string_view message, 
         json::value data = {}) {
    
    json::object log_entry;
    log_entry["timestamp"] = GetCurrentTimestamp();
    log_entry["message"] = message;
    
    if (!data.is_null()) {
        log_entry["data"] = data;
    }
    
    BOOST_LOG_STREAM_WITH_PARAMS(::boost::log::trivial::logger::get(), 
                                 (::boost::log::trivial::severity << level))
        << logging::add_value(additional_data, json::value(log_entry))
        << ""; // Пустая строка, так как вывод через форматтер
}

} // namespace logging