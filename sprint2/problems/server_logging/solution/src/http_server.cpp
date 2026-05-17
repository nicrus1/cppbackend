#include "http_server.h"

namespace http_server {

void SessionBase::Run() {
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, shared_from_this()));
}

void SessionBase::Read() {
    using namespace std::literals;
    req_ = {};
    stream_.expires_after(30s);
    http::async_read(stream_, buffer_, req_,
                     beast::bind_front_handler(&SessionBase::OnRead, shared_from_this()));
}

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
    using namespace std::literals;
    if (ec == http::error::end_of_stream) {
        return Close();
    }
    if (ec) {
        logger::LogError(ec.value(), ec.message(), "read");
        return ReportError(ec, "read"sv);
    }
    
    std::string target = std::string(req_.target());
    std::string method = std::string(req_.method_string());
    boost::json::object data;
    data["ip"] = client_ip_;
    data["URI"] = target;
    data["method"] = method;
    logger::LogRequestReceived(data);
    
    HandleRequest(std::move(req_));
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    if (ec) {
        logger::LogError(ec.value(), ec.message(), "shutdown");
    }
}

void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
    using namespace std::literals;
    if (ec) {
        logger::LogError(ec.value(), ec.message(), "write");
        return ReportError(ec, "write"sv);
    }

    if (close) {
        return Close();
    }

    Read();
}

}  // namespace http_server