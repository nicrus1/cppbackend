#pragma once

#include "logger.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

// Структуры данных для запроса и ответа
struct RequestData {
    std::string ip;
    std::string uri;
    std::string method;
};

struct ResponseData {
    int code;
    std::optional<std::string> content_type;
};

// Логирование запуска сервера
inline void LogServerStarted(uint16_t port, std::string_view address) {
    json::object data;
    data["port"] = port;
    data["address"] = address;
    LOG_INFO("server started", json::value(data));
}

// Логирование остановки сервера
inline void LogServerExited(int code, std::optional<std::string_view> exception = std::nullopt) {
    json::object data;
    data["code"] = code;
    if (exception.has_value()) {
        data["exception"] = json::value(exception.value().data());
    }
    LOG_INFO("server exited", json::value(data));
}

// Логирование ошибки
inline void LogError(int code, std::string_view text, std::string_view where) {
    json::object data;
    data["code"] = code;
    data["text"] = text;
    data["where"] = where;
    LOG_ERROR("error", json::value(data));
}

// Декоратор для логирования запросов и ответов
template<typename Handler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(Handler& handler) : decorated_(handler) {}

    ResponseData operator()(const RequestData& req) {
        // Логируем получение запроса
        json::object req_data;
        req_data["ip"] = json::value(req.ip);
        req_data["URI"] = json::value(req.uri);
        req_data["method"] = json::value(req.method);
        LOG_INFO("request received", json::value(req_data));

        // Измеряем время обработки
        auto start = std::chrono::steady_clock::now();
        ResponseData resp = decorated_(req);
        auto end = std::chrono::steady_clock::now();
        
        auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        // Логируем формирование ответа
        json::object resp_data;
        resp_data["ip"] = json::value(req.ip);
        resp_data["response_time"] = response_time;
        resp_data["code"] = resp.code;
        
        if (resp.content_type.has_value()) {
            resp_data["content_type"] = json::value(resp.content_type.value());
        } else {
            resp_data["content_type"] = json::value(nullptr);
        }
        
        LOG_INFO("response sent", json::value(resp_data));

        return resp;
    }

private:
    Handler& decorated_;
};