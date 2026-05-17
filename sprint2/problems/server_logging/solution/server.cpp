#include "logging_handler.hpp"

#include <iostream>
#include <thread>
#include <chrono>

// Пример обработчика запросов
class RequestHandler {
public:
    ResponseData operator()(const RequestData& req) {
        ResponseData resp;
        
        // Эмуляция обработки запроса
        if (req.uri == "/" || req.uri == "/index.html") {
            resp.code = 200;
            resp.content_type = "text/html";
        } else if (req.uri == "/api/health") {
            resp.code = 200;
            resp.content_type = "application/json";
        } else {
            resp.code = 404;
            resp.content_type = "text/html";
        }
        
        // Имитируем некоторую задержку обработки
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        
        return resp;
    }
};

// Функция запуска сервера
void RunServer(uint16_t port, std::string_view address) {
    RequestHandler handler;
    LoggingRequestHandler<RequestHandler> loggingHandler(handler);
    
    // Эмуляция получения и обработки запросов
    std::vector<RequestData> testRequests = {
        {"127.0.0.1", "/", "GET"},
        {"127.0.0.1", "/images/cube.svg", "GET"},
        {"127.0.0.1", "/file%20with+spaces.html", "GET"},
        {"127.0.0.1", "/abracadabra", "GET"},
        {"192.168.1.100", "/api/health", "GET"}
    };
    
    for (const auto& req : testRequests) {
        loggingHandler(req);
    }
}

int main() {
    // Инициализируем логгер
    InitLogger();
    
    uint16_t port = 8080;
    std::string address = "0.0.0.0";
    
    try {
        // Логируем запуск сервера
        LogServerStarted(port, address);
        
        // Запускаем сервер
        RunServer(port, address);
        
        // Логируем успешное завершение
        LogServerExited(0);
        
    } catch (const std::exception& e) {
        // Логируем завершение с ошибкой
        LogServerExited(EXIT_FAILURE, e.what());
        return EXIT_FAILURE;
    }
    
    return 0;
}