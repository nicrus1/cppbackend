#pragma once
#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/expressions.hpp>
#include <boost/json.hpp>
#include <string>

namespace logger {

void InitLogging();
void LogRequestReceived(const boost::json::object& data);
void LogResponseSent(const boost::json::object& data);
void LogServerStarted(const std::string& address, unsigned short port);
void LogServerExited(int code, const std::string& exception = "");
void LogError(int code, const std::string& text, const std::string& where);

}  // namespace logger