#pragma once
#include "sdk.h"
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <memory>
#include <functional>

namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

// Базовый класс для сессии
class SessionBase : public std::enable_shared_from_this<SessionBase> {
public:
    SessionBase(tcp::socket&& socket)
        : stream_(std::move(socket)) {
    }

    virtual ~SessionBase() = default;

    void Run() {
        net::dispatch(stream_.get_executor(),
                      beast::bind_front_handler(&SessionBase::ReadRequest, shared_from_this()));
    }

protected:
    void ReadRequest() {
        http::async_read(stream_, buffer_, req_,
            beast::bind_front_handler(&SessionBase::OnRead, shared_from_this()));
    }

    void OnRead(beast::error_code ec, size_t) {
        if (ec == http::error::end_of_stream) {
            return Close();
        }
        if (ec) {
            return;
        }
        HandleRequest(std::move(req_));
    }

    void Close() {
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    virtual void HandleRequest(http::request<http::string_body>&& req) = 0;

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
};

// Шаблонный класс сессии
template <typename RequestHandler>
class Session : public SessionBase {
public:
    Session(tcp::socket&& socket, RequestHandler handler)
        : SessionBase(std::move(socket))
        , handler_(std::move(handler)) {
    }

private:
    void HandleRequest(http::request<http::string_body>&& req) override {
        auto self = shared_from_this();
        handler_(std::move(req), [self](http::response<http::string_body>&& response) {
            auto keep_alive = response.keep_alive();
            http::async_write(self->stream_, response,
                [self, keep_alive](beast::error_code ec, size_t) {
                    if (ec) {
                        return;
                    }
                    if (!keep_alive) {
                        self->Close();
                    } else {
                        self->ReadRequest();
                    }
                });
        });
    }

    RequestHandler handler_;
};

// Класс слушателя
template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler handler)
        : ioc_(ioc)
        , acceptor_(ioc)
        , handler_(std::move(handler)) {
        
        beast::error_code ec;
        
        // Открываем акцептор
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            return;
        }
        
        // Разрешаем переиспользование адреса
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            return;
        }
        
        // Привязываемся к порту
        acceptor_.bind(endpoint, ec);
        if (ec) {
            return;
        }
        
        // Начинаем слушать
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            return;
        }
    }

    void Run() {
        DoAccept();
    }

private:
    void DoAccept() {
        acceptor_.async_accept(
            beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
    }

    void OnAccept(beast::error_code ec, tcp::socket socket) {
        if (ec) {
            return;
        }
        
        // Создаем сессию и запускаем её
        std::make_shared<Session<RequestHandler>>(std::move(socket), handler_)->Run();
        
        // Продолжаем принимать новые соединения
        DoAccept();
    }

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler handler_;
};

// Функция запуска сервера
template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    using HandlerType = std::decay_t<RequestHandler>;
    std::make_shared<Listener<HandlerType>>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}

}  // namespace http_server