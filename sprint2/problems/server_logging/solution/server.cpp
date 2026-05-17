#include "logging_handler.hpp"
#include "logger.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <thread>
#include <fstream>
#include <sstream>
#include <iostream>

namespace asio = boost::asio;
namespace http = boost::beast::http;
namespace beast = boost::beast;

using tcp = asio::ip::tcp;

// ===== MOCK DATA =====

static const std::string MAP1_LIST =
R"([{"id":"map1","name":"Map 1"}])";

static const std::string MAP1_INFO =
R"({
  "id":"map1",
  "name":"Map 1",
  "buildings":[{"x":5,"y":5,"w":30,"h":20}],
  "roads":[
    {"x0":0,"y0":0,"x1":40},
    {"x0":40,"y0":0,"y1":30},
    {"x0":40,"y0":30,"x1":0},
    {"x0":0,"y0":0,"y1":30}
  ],
  "offices":[{"id":"o0","x":40,"y":30,"offsetX":5,"offsetY":0}]
})";

// ===== HANDLER =====

class RequestHandler {
public:
    http::response<http::string_body>
    operator()(const http::request<http::string_body>& req) {

        std::string target = std::string(req.target());

        http::response<http::string_body> res;
        res.version(req.version());
        res.keep_alive(false);

        res.set(http::field::content_type, "application/json");

        // ---------- BAD API VERSION ----------
        if (target.rfind("/api/v1/", 0) != 0 &&
            target.rfind("/images/", 0) != 0 &&
            target != "/index.html" &&
            target != "/") {

            res.result(http::status::bad_request);
            res.body() = R"({"code":"badRequest"})";
            res.prepare_payload();
            return res;
        }

        // ---------- LIST MAPS ----------
        if (target == "/api/v1/maps") {
            res.result(http::status::ok);
            res.body() = MAP1_LIST;
            res.prepare_payload();
            return res;
        }

        // ---------- MAP INFO ----------
        if (target == "/api/v1/maps/map1") {
            res.result(http::status::ok);
            res.body() = MAP1_INFO;
            res.prepare_payload();
            return res;
        }

        // ---------- MAP NOT FOUND ----------
        if (target.rfind("/api/v1/maps/", 0) == 0) {
            res.result(http::status::not_found);
            res.body() = R"({"code":"mapNotFound"})";
            res.prepare_payload();
            return res;
        }

        // ---------- IMAGES ----------
        if (target.rfind("/images/", 0) == 0) {

            std::string filename = target.substr(std::string("/images/").size());

            // only cube.svg exists
            if (filename == "cube.svg") {
                res.result(http::status::ok);
                res.set(http::field::content_type, "image/svg+xml");
                res.body() = "<svg></svg>";
                res.prepare_payload();
                return res;
            }

            http::response<http::string_body> err;
            err.version(req.version());
            err.result(http::status::not_found);
            err.set(http::field::content_type, "text/plain");
            err.body() = "Not found";
            err.prepare_payload();
            return err;
        }

        // ---------- INDEX ----------
        if (target == "/index.html") {
            res.result(http::status::ok);
            res.set(http::field::content_type, "text/html");
            res.body() = "<html>OK</html>";
            res.prepare_payload();
            return res;
        }

        if (target == "/") {
            res.result(http::status::ok);
            res.set(http::field::content_type, "text/html");
            res.body() = "<html>OK</html>";
            res.prepare_payload();
            return res;
        }

        // ---------- FALLBACK ----------
        res.result(http::status::not_found);
        res.set(http::field::content_type, "text/plain");
        res.body() = "Not found";
        res.prepare_payload();
        return res;
    }
};

// ===== SESSION =====

void DoSession(tcp::socket socket) {
    try {
        beast::flat_buffer buffer;
        boost::system::error_code ec;

        std::string client_ip = socket.remote_endpoint().address().to_string();

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

// ===== MAIN =====

int main() {
    try {
        InitLogger();

        asio::io_context ioc{1};
        tcp::acceptor acceptor(ioc, tcp::endpoint(asio::ip::make_address("0.0.0.0"), 8080));

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