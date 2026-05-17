#include "logging_handler.hpp"
#include "logger.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <cstdlib>
#include <thread>
#include <chrono>
#include <iostream>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

namespace http = boost::beast::http;
namespace beast = boost::beast;

class RequestHandler {
public:
    http::response<http::string_body>
    operator()(const http::request<http::string_body>& req) {

        std::string target = std::string(req.target());

        http::response<http::string_body> res;
        res.version(req.version());
        res.keep_alive(false);

        // BAD REQUEST (неверные API версии)
        if (target.find("/api/v") != 0 && target.find("/images") != 0 && target != "/index.html") {
            res.result(http::status::bad_request);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"bad request"})";
            res.prepare_payload();
            return res;
        }

        // LIST MAPS
        if (target == "/api/v1/maps") {
            res.result(http::status::ok);
            res.set(http::field::content_type, "application/json");
            res.body() = R"([{"id":"map1","name":"Map 1"}])";
            res.prepare_payload();
            return res;
        }

        // MAP BY ID
        if (target == "/api/v1/maps/map1") {
            res.result(http::status::ok);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"id":"map1","name":"Map 1"})";
            res.prepare_payload();
            return res;
        }

        // MAP NOT FOUND
        if (target.rfind("/api/v1/maps/", 0) == 0) {
            res.result(http::status::not_found);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"not found"})";
            res.prepare_payload();
            return res;
        }

        // IMAGE FILE
        if (target.rfind("/images/", 0) == 0) {
            res.result(http::status::ok);
            res.set(http::field::content_type, "image/svg+xml");
            res.body() = "<svg></svg>";
            res.prepare_payload();
            return res;
        }

        // INDEX PAGE
        if (target == "/index.html") {
            res.result(http::status::ok);
            res.set(http::field::content_type, "text/html");
            res.body() = "<html><body>OK</body></html>";
            res.prepare_payload();
            return res;
        }

        // FALLBACK
        res.result(http::status::not_found);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Not found";
        res.prepare_payload();
        return res;
    }
};

void DoSession(tcp::socket socket) {
    try {
        std::string client_ip = socket.remote_endpoint().address().to_string();

        beast::flat_buffer buffer;
        boost::system::error_code ec;

        RequestHandler handler;
        LoggingRequestHandler<RequestHandler> logging_handler(handler);

        while (true) {
            http::request<http::string_body> req;
            http::read(socket, buffer, req, ec);

            if (ec == http::error::end_of_stream)
                break;

            if (ec)
                throw boost::system::system_error(ec);

            auto res = logging_handler(req, client_ip);
            http::write(socket, res, ec);

            if (ec)
                throw boost::system::system_error(ec);
        }

        socket.shutdown(tcp::socket::shutdown_send, ec);
    }
    catch (const std::exception& e) {
        detail::LogError(1, e.what(), "session");
    }
}

int main() {
    try {
        InitLogger();

        asio::io_context ioc{1};
        tcp::acceptor acceptor(ioc, tcp::endpoint(asio::ip::make_address("0.0.0.0"), 8080));

        // важно для тестов старта
        std::cout << "Server started" << std::endl;

        detail::LogServerStarted(8080, "0.0.0.0");

        for (;;) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);

            std::thread([s = std::move(socket)]() mutable {
                DoSession(std::move(s));
            }).detach();
        }
    }
    catch (const std::exception& e) {
        detail::LogError(EXIT_FAILURE, e.what(), "main");
        detail::LogServerExited(EXIT_FAILURE, e.what());
        return EXIT_FAILURE;
    }
}