#include "logging_handler.hpp"

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <thread>

namespace net = boost::asio;
namespace tcp = net::ip::tcp;
namespace http = boost::beast::http;
namespace beast = boost::beast;

using Clock = std::chrono::steady_clock;

http::response<http::string_body> HandleRequest(const http::request<http::string_body>& req) {
    RequestData request_data{
        "127.0.0.1",
        std::string(req.target()),
        std::string(req.method_string())
    };

    LogRequest(request_data);

    auto start = Clock::now();

    http::response<http::string_body> res;
    res.version(req.version());
    res.keep_alive(false);

    ResponseData response_data{};

    if (req.target() == "/api/v1/maps") {
        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"maps":[]})";

        response_data.code = 200;
        response_data.content_type = "application/json";
    } else {
        res.result(http::status::not_found);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Not found";

        response_data.code = 404;
        response_data.content_type = "text/plain";
    }

    res.prepare_payload();

    auto end = Clock::now();
    response_data.response_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    LogResponse(request_data, response_data);

    return res;
}

void DoSession(tcp::socket socket) {
    try {
        bool close = false;
        beast::error_code ec;
        beast::flat_buffer buffer;

        while (!close) {
            http::request<http::string_body> req;

            http::read(socket, buffer, req, ec);

            if (ec == http::error::end_of_stream)
                break;

            if (ec)
                throw beast::system_error(ec);

            auto res = HandleRequest(req);
            close = res.need_eof();

            http::write(socket, res, ec);

            if (ec)
                throw beast::system_error(ec);
        }

        socket.shutdown(tcp::socket::shutdown_send, ec);
    }
    catch (const std::exception& e) {
        LogError(1, e.what(), "session");
    }
}

int main() {
    try {
        InitLogger();

        const auto address = net::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;

        LogServerStarted(port, "0.0.0.0");

        net::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {address, port}};

        for (;;) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);

            std::thread([s = std::move(socket)]() mutable {
                DoSession(std::move(s));
            }).detach();
        }
    }
    catch (const std::exception& e) {
        LogError(EXIT_FAILURE, e.what(), "main");
        LogServerExited(EXIT_FAILURE, e.what());
        return EXIT_FAILURE;
    }

    LogServerExited(0);
    return EXIT_SUCCESS;
}