#include "http_server.h"
#include <iostream>

using namespace std::literals;
namespace net = boost::asio;
namespace http = boost::beast::http;
using tcp = net::ip::tcp;

http::response<http::string_body>
HandleRequest(http::request<http::string_body>&& req) {

    auto make_response = [&](http::status status,
                             std::string body,
                             std::string content_type = "text/html"s) {

        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::content_type, content_type);
        res.body() = std::move(body);
        res.content_length(res.body().size());
        return res;
    };

    std::string target = std::string(req.target());

    if (!target.empty() && target[0] == '/') {
        target.erase(0, 1);
    }

    if (req.method() == http::verb::get) {
        return make_response(http::status::ok, "Hello, " + target);
    }

    if (req.method() == http::verb::head) {
        auto res = make_response(http::status::ok, "Hello, " + target);
        res.body().clear();
        res.prepare_payload();
        return res;
    }

    http::response<http::string_body> res{http::status::method_not_allowed, req.version()};
    res.set(http::field::content_type, "text/html");
    res.set(http::field::allow, "GET, HEAD");
    res.body() = "Invalid method";
    res.content_length(res.body().size());
    return res;
}

int main() {
    net::io_context ioc;

    const auto address = net::ip::make_address("0.0.0.0");
    constexpr tcp::endpoint endpoint{address, 8080};

    http_server::ServeHttp(ioc, endpoint,
        [](auto&& req, auto&& sender) {
            sender(HandleRequest(std::forward<decltype(req)>(req)));
        });

    std::cout << "Server has started..." << std::endl;

    ioc.run();
}