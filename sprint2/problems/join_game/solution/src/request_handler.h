#pragma once
#include "http_server.h"
#include "model.h"
#include "api_handler.h"
#include "logger.h"
#include <boost/json.hpp>
#include <optional>

namespace http_handler {

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game}
        , api_handler_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);

private:
    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);

    template <typename Body, typename Allocator, typename Send>
    void SendResponse(http::request<Body, http::basic_fields<Allocator>>&& req,
                      Send&& send,
                      http::status status,
                      std::string_view content_type,
                      std::string_view body);

    template <typename Body, typename Allocator, typename Send>
    void SendError(http::request<Body, http::basic_fields<Allocator>>&& req,
                   Send&& send,
                   http::status status,
                   std::string_view code,
                   std::string_view message);

    std::string SerializeMaps() const;
    std::string SerializeMap(const model::Map& map) const;
    boost::json::array SerializeRoads(const model::Map& map) const;
    boost::json::array SerializeBuildings(const model::Map& map) const;
    boost::json::array SerializeOffices(const model::Map& map) const;

    model::Game& game_;
    ApiHandler api_handler_;
};

} // namespace http_handler