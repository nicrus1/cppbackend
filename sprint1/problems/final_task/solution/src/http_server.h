#pragma once
#include "sdk.h"
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <memory>

namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

inline void ReportError(beast::error_code ec, std::string_view what) {
    std::cerr << what << ": " << ec.message() << std::endl;
}

class SessionBase : public std::enable_shared_from_this<SessionBase> {
public:
    explicit SessionBase(tcp::socket&& socket);
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
        request_handler_(std::move(request), [self = this->shared_from_this()](auto&& response) {
            self->Write(std::move(response));
        });
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
            std::cerr << "Open error: " << ec.message() << std::endl;
            return;
        }
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            std::cerr << "Set option error: " << ec.message() << std::endl;
            return;
        }
        acceptor_.bind(endpoint, ec);
        if (ec) {
            std::cerr << "Bind error: " << ec.message() << std::endl;
            return;
        }
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            std::cerr << "Listen error: " << ec.message() << std::endl;
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
                    std::cerr << "Accept error: " << ec.message() << std::endl;
                    return;
                }
                std::make_shared<Session<RequestHandler>>(std::move(socket), self->handler_)->Run();
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