#pragma once

#include "logger.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <boost/beast/http.hpp>

namespace logging = boost::log;
namespace json = boost::json;
namespace http = boost::beast::http;

struct RequestData {
    std::string ip;
    std::string uri;
    std::string method;
};

struct ResponseData {
    int code{};
    std::optional<std::string> content_type;
    int response_time{};
};

// Вспомогательные функции логирования (внутренние)
namespace detail {
    inline void LogServerStarted(uint16_t port, std::string_view address) {
        json::object data;
        data["port"] = port;
        data["address"] = std::string(address);

        BOOST_LOG_TRIVIAL(info)
            << logging::add_value(message_attr, std::string("server started"))
            << logging::add_value(data_attr, data);
    }

    inline void LogServerExited(int code, const std::string& exception = "") {
        json::object data;
        data["code"] = code;
        if (!exception.empty())
            data["exception"] = exception;

        BOOST_LOG_TRIVIAL(info)
            << logging::add_value(message_attr, std::string("server exited"))
            << logging::add_value(data_attr, data);
    }

    inline void LogError(int code, const std::string& text, const std::string& where) {
        json::object data;
        data["code"] = code;
        data["text"] = text;
        data["where"] = where;

        BOOST_LOG_TRIVIAL(error)
            << logging::add_value(message_attr, std::string("error"))
            << logging::add_value(data_attr, data);
    }
}

// Декоратор для логирования запросов и ответов
template<typename Handler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(Handler& handler) : decorated_(handler) {}

    auto operator()(const http::request<http::string_body>& req, 
                    const std::string& client_ip) {
        // Логируем запрос
        LogRequest(req, client_ip);
        
        auto start = std::chrono::steady_clock::now();
        
        // Вызываем реальный обработчик
        auto res = decorated_(req);
        
        auto end = std::chrono::steady_clock::now();
        auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        // Логируем ответ
        LogResponse(res, client_ip, response_time);
        
        return res;
    }

private:
    void LogRequest(const http::request<http::string_body>& req, const std::string& client_ip) {
        json::object data;
        data["ip"] = client_ip;
        data["URI"] = std::string(req.target());
        data["method"] = std::string(req.method_string());

        BOOST_LOG_TRIVIAL(info)
            << logging::add_value(message_attr, std::string("request received"))
            << logging::add_value(data_attr, data);
    }

    void LogResponse(const http::response<http::string_body>& res, 
                     const std::string& client_ip, 
                     int response_time) {
        json::object data;
        data["ip"] = client_ip;
        data["code"] = res.result_int();
        data["response_time"] = response_time;
        
        auto content_type = res.find(http::field::content_type);
        if (content_type != res.end()) {
            data["content_type"] = std::string(content_type->value());
        } else {
            data["content_type"] = nullptr;
        }

        BOOST_LOG_TRIVIAL(info)
            << logging::add_value(message_attr, std::string("response sent"))
            << logging::add_value(data_attr, data);
    }

    Handler& decorated_;
};