#pragma once
#include "model.h"
#include "players.h"
#include "player_tokens.h"
#include "game_session.h"
#include <boost/json.hpp>
#include <string>
#include <functional>
#include <memory>

namespace http_handler {

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game);
    
    void operator()(http::request<http::string_body>&& req, 
                    std::function<void(http::response<http::string_body>&&)>&& send);
    
private:
    std::string SerializeMaps() const;
    std::string SerializeMap(const model::Map& map) const;
    boost::json::array SerializeRoads(const model::Map& map) const;
    boost::json::array SerializeBuildings(const model::Map& map) const;
    boost::json::array SerializeOffices(const model::Map& map) const;
    
    model::Game& game_;
    game::GameSession session_;
};

}  // namespace http_handler