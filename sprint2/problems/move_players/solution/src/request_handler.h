#pragma once
#include "http_server.h"
#include "model.h"
#include "api_handler.h"
#include "logger.h"
#include <boost/json.hpp>
#include <optional>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game}
        , api_handler_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target = std::string(req.target());
        std::string method = std::string(req.method_string());

        auto query_pos = target.find('?');
        if (query_pos != std::string::npos) {
            target = target.substr(0, query_pos);
        }

        // JOIN endpoint
        if (target == "/api/v1/game/join") {
            api_handler_.HandleJoin(std::move(req), std::forward<Send>(send));
            return;
        }

        // PLAYERS endpoint
        if (target == "/api/v1/game/players") {
            api_handler_.HandleGetPlayers(std::move(req), std::forward<Send>(send));
            return;
        }

        // GAME STATE endpoint
        if (target == "/api/v1/game/state") {
            api_handler_.HandleGameState(std::move(req), std::forward<Send>(send));
            return;
        }
        
        // PLAYER ACTION endpoint
        if (target == "/api/v1/game/player/action") {
            api_handler_.HandlePlayerAction(std::move(req), std::forward<Send>(send));
            return;
        }

        // MAPS endpoints
        if (target == "/api/v1/maps") {
            if (method != "GET") {
                SendError(std::move(req), send, http::status::method_not_allowed,
                          "badRequest", "Method not allowed");
                return;
            }
            std::string body = SerializeMaps();
            SendResponse(std::move(req), send, http::status::ok, "application/json", body);
            return;
        }

        // Single map endpoint
        const std::string prefix = "/api/v1/maps/";
        if (target.find(prefix) == 0) {
            if (method != "GET") {
                SendError(std::move(req), send, http::status::method_not_allowed,
                          "badRequest", "Method not allowed");
                return;
            }
            std::string map_id_str = target.substr(prefix.length());
            
            while (!map_id_str.empty() && map_id_str.back() == '/') {
                map_id_str.pop_back();
            }

            auto query_pos = map_id_str.find('?');
            if (query_pos != std::string::npos) {
                map_id_str = map_id_str.substr(0, query_pos);
            }

            model::Map::Id map_id{std::move(map_id_str)};
            const model::Map* map = game_.FindMap(map_id);

            if (!map) {
                SendError(std::move(req), send, http::status::not_found,
                          "mapNotFound", "Map not found");
                return;
            }

            std::string body = SerializeMap(*map);
            SendResponse(std::move(req), send, http::status::ok, "application/json", body);
            return;
        }

        // Not found
        SendError(std::move(req), send, http::status::not_found, "badRequest", "Not found");
    }

private:
    template <typename Body, typename Allocator, typename Send>
    void SendResponse(http::request<Body, http::basic_fields<Allocator>>&& req,
                      Send&& send,
                      http::status status,
                      std::string_view content_type,
                      std::string_view body) {
        http::response<http::string_body> response(status, req.version());
        response.set(http::field::content_type, content_type);
        response.body() = body;
        response.prepare_payload();
        response.keep_alive(req.keep_alive());
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void SendError(http::request<Body, http::basic_fields<Allocator>>&& req,
                   Send&& send,
                   http::status status,
                   std::string_view code,
                   std::string_view message) {
        std::string body = boost::json::serialize(
            boost::json::object{
                {"code", code},
                {"message", message}
            });
        
        http::response<http::string_body> response(status, req.version());
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.body() = body;
        response.prepare_payload();
        response.keep_alive(req.keep_alive());
        send(std::move(response));
    }

    std::string SerializeMaps() const;
    std::string SerializeMap(const model::Map& map) const;
    boost::json::array SerializeRoads(const model::Map& map) const;
    boost::json::array SerializeBuildings(const model::Map& map) const;
    boost::json::array SerializeOffices(const model::Map& map) const;

    model::Game& game_;
    ApiHandler api_handler_;
};

} // namespace http_handler