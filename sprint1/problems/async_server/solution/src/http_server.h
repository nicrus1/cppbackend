#pragma once
#include "sdk.h"

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <memory>
#include <utility>

namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

class SessionBase : public std::enable_shared_from_this<SessionBase> {
public:
    explicit SessionBase(tcp::socket&& socket)
        : stream_(std::move(socket)) {}

    SessionBase(const SessionBase&) = delete;
    SessionBase& operator=(const SessionBase&) = delete;

    void Run() {
        net::dispatch(stream_.get_executor(),
            beast::bind_front_handler(&SessionBase::Read, shared_from_this()));
    }

protected:
    void Close() {
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    beast::tcp_stream& Stream() {
        return stream_;
    }

    virtual void HandleRequest(http::request<http::string_body>&& req) = 0;

private:
    void Read() {
        request_ = {};
        stream_.expires_after(std::chrono::seconds(30));

        http::async_read(
            stream_,
            buffer_,
            request_,
            beast::bind_front_handler(&SessionBase::OnRead, shared_from_this())
        );
    }

    void OnRead(beast::error_code ec, std::size_t) {
        if (ec == http::error::end_of_stream) {
            return Close();
        }
        if (ec) {
            return;
        }

        HandleRequest(std::move(request_));
    }

protected:
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
};

template <typename RequestHandler>
class Session : public SessionBase {
public:
    Session(tcp::socket&& socket, RequestHandler handler)
        : SessionBase(std::move(socket))
        , handler_(std::move(handler)) {}

private:
    void HandleRequest(http::request<http::string_body>&& req) override {
        auto self = std::static_pointer_cast<Session>(shared_from_this());

        handler_(std::move(req),
            [self](http::response<http::string_body> response) mutable {
                bool keep_alive = response.keep_alive();

                http::async_write(
                    self->Stream(),
                    response,
                    [self, keep_alive](beast::error_code ec, std::size_t) {
                        if (!ec && !keep_alive) {
                            self->Close();
                            return;
                        }
                        if (!ec) {
                            self->Run();
                        }
                    }
                );
            }
        );
    }

    RequestHandler handler_;
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler handler)
        : ioc_(ioc)
        , acceptor_(ioc)
        , handler_(std::move(handler)) {

        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec) return;

        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) return;

        acceptor_.bind(endpoint, ec);
        if (ec) return;

        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) return;
    }

    void Run() {
        DoAccept();
    }

private:
    void DoAccept() {
        acceptor_.async_accept(
            [self = this->shared_from_this()](beast::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session<RequestHandler>>(
                        std::move(socket),
                        self->handler_
                    )->Run();
                }
                self->DoAccept();
            });
    }

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler handler_;
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc,
               const tcp::endpoint& endpoint,
               RequestHandler handler) {
    std::make_shared<Listener<std::decay_t<RequestHandler>>>(
        ioc, endpoint, std::move(handler)
    )->Run();
}

} // namespace http_server