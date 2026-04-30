#ifdef WIN32
#include <sdkddkver.h>
#endif
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <thread>
#include <optional>

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;
namespace beast = boost::beast;
namespace http = beast::http;

// Функция обработки запроса
http::response<http::string_body> HandleRequest(const http::request<http::string_body>& req) {
    http::response<http::string_body> res;
    
    // Проверяем метод запроса
    if (req.method() == http::verb::get) {
        // Обработка GET запроса
        std::string target = std::string(req.target());
        
        // Удаляем ведущий символ '/'
        std::string name;
        if (target == "/") {
            name = "";
        } else if (!target.empty() && target[0] == '/') {
            name = target.substr(1);
        } else {
            name = target;
        }
        
        // Формируем тело ответа
        std::string body = "Hello, " + name;
        
        res.result(http::status::ok);
        res.set(http::field::content_type, "text/html");
        res.body() = body;
        res.set(http::field::content_length, std::to_string(body.size()));
        
    } else if (req.method() == http::verb::head) {
        // Обработка HEAD запроса - тело пустое, но Content-Length как у GET
        std::string target = std::string(req.target());
        
        // Удаляем ведущий символ '/'
        std::string name;
        if (target == "/") {
            name = "";
        } else if (!target.empty() && target[0] == '/') {
            name = target.substr(1);
        } else {
            name = target;
        }
        
        // Вычисляем длину тела (как если бы это был GET запрос)
        std::string body_content = "Hello, " + name;
        
        res.result(http::status::ok);
        res.set(http::field::content_type, "text/html");
        res.body() = "";
        res.set(http::field::content_length, std::to_string(body_content.size()));
        
    } else {
        // Обработка других методов (405 Method Not Allowed)
        // ВАЖНО: тело ответа "Invalid method" БЕЗ точки в конце
        std::string body = "Invalid method";
        
        res.result(http::status::method_not_allowed);
        res.set(http::field::content_type, "text/html");
        res.set(http::field::allow, "GET, HEAD");
        res.body() = body;
        res.set(http::field::content_length, std::to_string(body.size()));
    }
    
    // Устанавливаем версию HTTP (оставляем ту же, что и в запросе)
    res.version(req.version());
    
    return res;
}

int main() {
    try {
        // Создаём io_context
        net::io_context ioc(1);
        
        // Адрес и порт
        auto address = net::ip::make_address("0.0.0.0");
        unsigned short port = 8080;
        
        // Создаём acceptor
        tcp::acceptor acceptor(ioc, {address, port});
        
        // Выводим сообщение о готовности сервера
        std::cout << "Server has started..."sv << std::endl;
        
        // Основной цикл обработки запросов
        while (true) {
            // Принимаем соединение
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            
            // Буфер для чтения
            beast::flat_buffer buffer;
            
            // Читаем запрос
            http::request<http::string_body> req;
            beast::error_code ec;
            http::read(socket, buffer, req, ec);
            
            if (ec) {
                continue;
            }
            
            // Обрабатываем запрос
            auto res = HandleRequest(req);
            
            // Отправляем ответ
            http::write(socket, res, ec);
            if (ec) {
                continue;
            }
            
            // Закрываем соединение
            socket.shutdown(tcp::socket::shutdown_send, ec);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}