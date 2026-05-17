#pragma once

#include "request_handler.h"
#include "logger.h"
#include <chrono>
#include <boost/beast/http.hpp>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

template<typename RequestHandler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(RequestHandler& handler) 
        : decorated_(handler) {}
    
    void operator()(http::request<http::string_body>&& req, 
                   std::function<void(http::response<http::string_body>)>&& send) {
        // Логируем получение запроса
        LogRequest(req);
        
        // Засекаем время начала обработки
        auto start_time = std::chrono::steady_clock::now();
        
        // Вызываем оригинальный обработчик с обёрткой для ответа
        decorated_(std::move(req), [this, send = std::move(send), start_time]
                  (http::response<http::string_body> response) {
            // Логируем отправку ответа
            LogResponse(response, start_time);
            send(std::move(response));
        });
    }
    
private:
    void LogRequest(const http::request<http::string_body>& req) {
        boost::json::object data;
        
        // Получаем IP адрес (нужно будет передавать через контекст)
        // Пока оставляем заглушку, IP будет добавлен позже
        data["URI"] = std::string(req.target());
        data["method"] = std::string(req.method_string());
        
        logging::Log(boost::log::trivial::info, "request received", 
                     boost::json::value(data));
    }
    
    void LogResponse(const http::response<http::string_body>& res,
                    const std::chrono::steady_clock::time_point& start_time) {
        auto end_time = std::chrono::steady_clock::now();
        auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
        boost::json::object data;
        data["response_time"] = static_cast<std::int64_t>(response_time);
        data["code"] = res.result_int();
        
        // Получаем Content-Type из заголовка
        auto content_type = res.find(http::field::content_type);
        if (content_type != res.end()) {
            data["content_type"] = std::string(content_type->value());
        } else {
            data["content_type"] = nullptr;
        }
        
        logging::Log(boost::log::trivial::info, "response sent", 
                     boost::json::value(data));
    }
    
    RequestHandler& decorated_;
};

} // namespace http_handler