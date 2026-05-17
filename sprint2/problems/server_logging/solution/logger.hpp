#pragma once

#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/json.hpp>
#include <boost/json/value.hpp>
#include <boost/json/serialize.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace logging = boost::log;
namespace sinks = logging::sinks;
namespace src = logging::sources;
namespace expr = logging::expressions;
namespace keywords = logging::keywords;
namespace json = boost::json;

using namespace std::literals;

// Ключевые слова для атрибутов
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", std::chrono::system_clock::time_point)
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

// Инициализация логгера с JSON-форматом
inline void InitLogger() {
    logging::add_common_attributes();

    auto sink = logging::add_console_log<std::ostream>(std::cout);

    sink->set_formatter([](const logging::record_view& rec, logging::formatting_ostream& strm) {
        json::object obj;

        // timestamp в ISO формате
        auto ts = rec[timestamp];
        if (ts) {
            auto time_point = ts.get();
            auto time_t = std::chrono::system_clock::to_time_t(time_point);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                time_point.time_since_epoch()) % 1000;
            
            std::tm tm;
#ifdef _WIN32
            gmtime_s(&tm, &time_t);
#else
            gmtime_r(&time_t, &tm);
#endif
            
            std::stringstream ss;
            ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") 
               << '.' << std::setfill('0') << std::setw(3) << ms.count();
            obj["timestamp"] = json::value(ss.str());
        }

        // message
        auto msg = rec[logging::expressions::smessage];
        if (msg) {
            obj["message"] = json::value(msg.get().c_str());
        }

        // data
        auto data = rec[additional_data];
        if (data) {
            obj["data"] = data.get();
        } else {
            obj["data"] = json::object();
        }

        strm << json::serialize(json::value(obj));
    });
}

// Макросы для логирования
#define LOG_INFO(message, data) \
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << message

#define LOG_ERROR(message, data) \
    BOOST_LOG_TRIVIAL(error) << logging::add_value(additional_data, data) << message