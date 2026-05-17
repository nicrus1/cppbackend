#include "logger.h"
#include <iostream>
#include <chrono>

namespace logger {

namespace logging = boost::log;
namespace expr = boost::log::expressions;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)

void InitLogging() {
    logging::add_common_attributes();
    auto sink = logging::add_console_log(std::cout);
    sink->set_formatter(
        expr::stream << expr::smessage
    );
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