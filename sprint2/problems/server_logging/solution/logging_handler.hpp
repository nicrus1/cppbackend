#pragma once

#include "logger.hpp"

#include <optional>
#include <string>
#include <string_view>

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

inline void LogServerStarted(uint16_t port, std::string_view address) {
    json::object data;
    data["port"] = port;
    data["address"] = std::string(address);

    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(message_attr, "server started")
        << logging::add_value(data_attr, data);
}

inline void LogServerExited(int code, const std::string& exception = "") {
    json::object data;
    data["code"] = code;
    if (!exception.empty())
        data["exception"] = exception;

    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(message_attr, "server exited")
        << logging::add_value(data_attr, data);
}

inline void LogRequest(const RequestData& req) {
    json::object data;
    data["ip"] = req.ip;
    data["URI"] = req.uri;
    data["method"] = req.method;

    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(message_attr, "request received")
        << logging::add_value(data_attr, data);
}

inline void LogResponse(const RequestData& req, const ResponseData& resp) {
    json::object data;
    data["ip"] = req.ip;
    data["code"] = resp.code;
    data["response_time"] = resp.response_time;

    if (resp.content_type)
        data["content_type"] = *resp.content_type;
    else
        data["content_type"] = nullptr;

    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(message_attr, "response sent")
        << logging::add_value(data_attr, data);
}

inline void LogError(int code, const std::string& text, const std::string& where) {
    json::object data;
    data["code"] = code;
    data["text"] = text;
    data["where"] = where;

    BOOST_LOG_TRIVIAL(error)
        << logging::add_value(message_attr, "error")
        << logging::add_value(data_attr, data);
}