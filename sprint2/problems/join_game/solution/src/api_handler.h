#pragma once
#include "model.h"
#include "game_state.h"
#include "http_server.h"
#include "logger.h"
#include <boost/json.hpp>
#include <optional>
#include <string>

namespace http_handler {

class ApiHandler {
public:
    explicit ApiHandler(model::Game& game) : game_state_(game) {}

    // POST /api/v1/game/join
    template <typename Body, typename Allocator, typename Send>
    void HandleJoin(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);

    // GET/HEAD /api/v1/game/players
    template <typename Body, typename Allocator, typename Send>
    void HandleGetPlayers(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);

    // GET /api/v1/maps
    std::string HandleGetMaps(const model::Game& game) const;

    // GET /api/v1/maps/{id}
    std::optional<std::string> HandleGetMap(const model::Game& game, const std::string& map_id) const;

private:
    template <typename Body, typename Allocator>
    std::optional<model::Token> ExtractToken(
        const http::request<Body, http::basic_fields<Allocator>>& req);

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

    game::GameState game_state_;
};

} // namespace http_handler