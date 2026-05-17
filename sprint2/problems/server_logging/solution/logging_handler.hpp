#pragma once

#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

#include <chrono>
#include <string>

namespace http = boost::beast::http;
namespace json = boost::json;

// атрибуты логирования (ВАЖНО для тестов)
static constexpr const char* message_attr = "message";
static constexpr const char* data_attr = "data";

// ===== SYSTEM LOGS =====

namespace detail {

inline void LogServerStarted(uint16_t port, std::string_view address) {
    json::object data;
    data["port"] = port;
    data["address"] = std::string(address);

    BOOST_LOG_TRIVIAL(info)
        << boost::log::add_value(message_attr, std::string("Server started"))
        << boost::log::add_value(data_attr, data);
}

inline void LogServerExited(int code, const std::string& exception = "") {
    json::object data;
    data["code"] = code;
    if (!exception.empty())
        data["exception"] = exception;

    BOOST_LOG_TRIVIAL(info)
        << boost::log::add_value(message_attr, std::string("Server exited"))
        << boost::log::add_value(data_attr, data);
}

inline void LogError(int code, const std::string& text, const std::string& where) {
    json::object data;
    data["code"] = code;
    data["text"] = text;
    data["where"] = where;

    BOOST_LOG_TRIVIAL(error)
        << boost::log::add_value(message_attr, std::string("error"))
        << boost::log::add_value(data_attr, data);
}

} // namespace detail

// ===== REQUEST LOGGER WRAPPER =====

template<typename Handler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(Handler& handler)
        : decorated_(handler) {}

    http::response<http::string_body>
    operator()(const http::request<http::string_body>& req,
               const std::string& client_ip) {

        LogRequest(req, client_ip);

        auto start = std::chrono::steady_clock::now();

        auto res = decorated_(req);

        auto end = std::chrono::steady_clock::now();

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        LogResponse(res, client_ip, ms);

        return res;
    }

private:

    void LogRequest(const http::request<http::string_body>& req,
                    const std::string& ip) {

        json::object data;
        data["ip"] = ip;
        data["URI"] = std::string(req.target());
        data["method"] = std::string(req.method_string());

        BOOST_LOG_TRIVIAL(info)
            << boost::log::add_value(message_attr, std::string("request received"))
            << boost::log::add_value(data_attr, data);
    }

    void LogResponse(const http::response<http::string_body>& res,
                     const std::string& ip,
                     int ms) {

        json::object data;
        data["ip"] = ip;
        data["code"] = res.result_int();
        data["response_time"] = ms;

        auto ct = res.find(http::field::content_type);
        if (ct != res.end())
            data["content_type"] = std::string(ct->value());
        else
            data["content_type"] = nullptr;

        BOOST_LOG_TRIVIAL(info)
            << boost::log::add_value(message_attr, std::string("response sent"))
            << boost::log::add_value(data_attr, data);
    }

    Handler& decorated_;
};