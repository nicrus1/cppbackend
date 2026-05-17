#pragma once
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