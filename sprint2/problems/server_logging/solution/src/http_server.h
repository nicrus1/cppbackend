#pragma once
#include "sdk.h"
#include "logger.h"
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <memory>
#include <chrono>
#include "logger.h"

namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

inline void ReportError(beast::error_code ec, std::string_view what) {
    std::cerr << what << ": " << ec.message() << std::endl;
    logger::LogError(ec.value(), ec.message(), std::string(what));
}

class SessionBase : public std::enable_shared_from_this<SessionBase> {
public:
    explicit SessionBase(tcp::socket&& socket)
        : stream_(std::move(socket))
        , client_ip_(stream_.socket().remote_endpoint().address().to_string()) {
    }
    virtual ~SessionBase() = default;

    void Run();

    template <typename Body, typename Fields>
    void Write(http::response<Body, Fields>&& response) {
        auto safe_response = std::make_shared<http::response<Body, Fields>>(std::move(response));

        auto self = shared_from_this();
        http::async_write(stream_, *safe_response,
                          [safe_response, self](beast::error_code ec, std::size_t bytes_written) {
                              self->OnWrite(safe_response->need_eof(), ec, bytes_written);
                          });
    }
    
    const std::string& GetClientIp() const {
        return client_ip_;
    }

private:
    void Read();
    void OnRead(beast::error_code ec, std::size_t bytes_read);
    void Close();
    void OnWrite(bool close, beast::error_code ec, std::size_t bytes_written);

protected:
    virtual void HandleRequest(http::request<http::string_body>&& req) = 0;

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    std::string client_ip_;
};

template <typename RequestHandler>
class Session : public SessionBase {
public:
    template <typename Handler>
    Session(tcp::socket&& socket, Handler&& request_handler)
        : SessionBase(std::move(socket))
        , request_handler_(std::forward<Handler>(request_handler)) {
    }

private:
    void HandleRequest(http::request<http::string_body>&& request) override {
        // Передаём IP клиента через дополнительный параметр
        // Для этого используем замыкание
        auto start_time = std::chrono::steady_clock::now();
        std::string client_ip = GetClientIp();
        
        request_handler_(
            std::move(request), 
            [self = this->shared_from_this(), client_ip, start_time](auto&& response) {
                auto end_time = std::chrono::steady_clock::now();
                auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
                
                // Логируем отправку ответа
                std::string content_type = "null";
                auto it = response.find(http::field::content_type);
                if (it != response.end()) {
                    content_type = std::string(it->value());
                }
                
                boost::json::object data;
                data["ip"] = client_ip;
                data["response_time"] = response_time;
                data["code"] = response.result_int();
                if (content_type == "null") {
                    data["content_type"] = nullptr;
                } else {
                    data["content_type"] = content_type;
                }
                logger::LogResponseSent(data);
                
                self->Write(std::move(response));
            }
        );
    }

    RequestHandler request_handler_;
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler)
        : ioc_(ioc)
        , acceptor_(ioc)
        , handler_(std::forward<RequestHandler>(handler)) {
        beast::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            logger::LogError(ec.value(), ec.message(), "open");
            return;
        }
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            logger::LogError(ec.value(), ec.message(), "set_option");
            return;
        }
        acceptor_.bind(endpoint, ec);
        if (ec) {
            logger::LogError(ec.value(), ec.message(), "bind");
            return;
        }
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            logger::LogError(ec.value(), ec.message(), "listen");
            return;
        }
    }

    void Run() {
        DoAccept();
    }

private:
    void DoAccept() {
        acceptor_.async_accept(
            [self = this->shared_from_this()](beast::error_code ec, tcp::socket socket) {
                if (ec) {
                    logger::LogError(ec.value(), ec.message(), "accept");
                    self->DoAccept();
                    return;
                }
                auto session = std::make_shared<Session<RequestHandler>>(std::move(socket), self->handler_);
                
                // Логируем получение запроса (фактическое логирование будет при обработке запроса)
                session->Run();
                self->DoAccept();
            });
    }

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler handler_;
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    using MyListener = Listener<std::decay_t<RequestHandler>>;
    std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}

}  // namespace http_server