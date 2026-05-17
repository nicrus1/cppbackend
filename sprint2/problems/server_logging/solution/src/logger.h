#pragma once
#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/json.hpp>
#include <string>

namespace logger {

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;
namespace attrs = boost::log::attributes;

// Ключевое слово для дополнительных данных
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)

// Инициализация логгера
void InitLogging();

// Логирование получения запроса
void LogRequestReceived(const boost::json::object& data);

// Логирование отправки ответа
void LogResponseSent(const boost::json::object& data);

// Логирование запуска сервера
void LogServerStarted(const std::string& address, unsigned short port);

// Логирование остановки сервера
void LogServerExited(int code, const std::string& exception = "");

// Логирование ошибки
void LogError(int code, const std::string& text, const std::string& where);

}  // namespace logger