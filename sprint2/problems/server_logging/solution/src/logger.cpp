#include "logger.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <boost/log/attributes/constant.hpp>
#include <boost/log/attributes/scoped_attribute.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>

namespace logger {

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;
namespace attrs = boost::log::attributes;

void InitLogging() {
    // Добавляем общие атрибуты
    logging::add_common_attributes();
    
    // Настраиваем консольный вывод
    auto console_sink = logging::add_console_log(std::cout);
    
    // Задаём формат для консольного логгера
    console_sink->set_formatter(
        expr::stream << expr::smessage
    );
    
    // Устанавливаем уровень логирования
    logging::core::get()->set_filter(
        logging::trivial::severity >= logging::trivial::info
    );
}

void LogRequestReceived(const boost::json::object& data) {
    boost::json::value json_data(data);
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, json_data)
                            << "request received";
}

void LogResponseSent(const boost::json::object& data) {
    boost::json::value json_data(data);
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, json_data)
                            << "response sent";
}

void LogServerStarted(const std::string& address, unsigned short port) {
    boost::json::object data;
    data["address"] = address;
    data["port"] = port;
    boost::json::value json_data(data);
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, json_data)
                            << "server started";
}

void LogServerExited(int code, const std::string& exception) {
    boost::json::object data;
    data["code"] = code;
    if (!exception.empty()) {
        data["exception"] = exception;
    }
    boost::json::value json_data(data);
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, json_data)
                            << "server exited";
}

void LogError(int code, const std::string& text, const std::string& where) {
    boost::json::object data;
    data["code"] = code;
    data["text"] = text;
    data["where"] = where;
    boost::json::value json_data(data);
    BOOST_LOG_TRIVIAL(error) << logging::add_value(additional_data, json_data)
                             << "error";
}

}  // namespace logger