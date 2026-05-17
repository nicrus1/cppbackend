#pragma once

#include "logger.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <chrono>
#include <string>

namespace json = boost::json;
namespace http = boost::beast::http;

// ---------------- LOGGING DECORATOR ----------------

template<typename Handler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(Handler& handler)
        : decorated_(handler) {}

    auto operator()(const http::request<http::string_body>& req,
                    const std::string& client_ip) {

        LogRequest(req, client_ip);

        auto start = std::chrono::steady_clock::now();
        auto res = decorated_(req);
        auto end = std::chrono::steady_clock::now();

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        LogResponse(res, client_ip, (int)ms);

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
                     int response_time) {

        json::object data;
        data["ip"] = ip;
        data["code"] = res.result_int();
        data["response_time"] = response_time;

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