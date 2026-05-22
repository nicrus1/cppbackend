#pragma once

#include "http_server.h"
#include "api_handler.h"
#include "model.h"
#include "game_state.h"

#include <boost/json.hpp>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game}
        , game_state_{game}
        , api_handler_{game_state_} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body,
                    http::basic_fields<Allocator>>&& req,
                    Send&& send) {

        std::string target = std::string(req.target());

        if (target == "/api/v1/game/join") {
            api_handler_.HandleJoin(
                std::move(req),
                send
            );
            return;
        }

        if (target == "/api/v1/game/players") {
            api_handler_.HandleGetPlayers(
                std::move(req),
                send
            );
            return;
        }

        if (target == "/api/v1/game/state") {
            api_handler_.HandleGameState(
                std::move(req),
                send
            );
            return;
        }
    }

private:
    model::Game& game_;
    game::GameState game_state_;
    ApiHandler api_handler_;
};

} // namespace http_handler