#include "game_state.h"

GameState::GameState(model::Game& game)
    : game_(game) {
}

JoinGameResult GameState::JoinGame(const model::Map::Id& map_id,
                                   const std::string& user_name) {

    const model::Map* map = game_.FindMap(map_id);

    if (!map) {
        throw std::runtime_error("Map not found");
    }

    Player& player = players_.AddPlayer(user_name, map_id);

    const auto& roads = map->GetRoads();

    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);

    const auto& road = roads[road_dist(gen)];

    model::Position pos;

    if (road.IsHorizontal()) {
        auto start = road.GetStart();
        auto end = road.GetEnd();

        double min_x = std::min(start.x, end.x);
        double max_x = std::max(start.x, end.x);

        std::uniform_real_distribution<double> x_dist(min_x, max_x);

        pos.x = x_dist(gen);
        pos.y = start.y;
    } else {
        auto start = road.GetStart();
        auto end = road.GetEnd();

        double min_y = std::min(start.y, end.y);
        double max_y = std::max(start.y, end.y);

        std::uniform_real_distribution<double> y_dist(min_y, max_y);

        pos.x = start.x;
        pos.y = y_dist(gen);
    }

    player.SetPosition(pos);
    player.SetSpeed({0.0, 0.0});
    player.SetDirection(model::Direction::NORTH);

    Token token = tokens_.AddPlayer(player.GetId());

    return {token, player.GetId()};
}

boost::json::object GameState::GetGameState(const Token& token) {
    if (!tokens_.IsValidToken(token)) {
        throw std::runtime_error("Invalid token");
    }

    PlayerId player_id = tokens_.FindPlayerByToken(token);

    Player* player = players_.FindById(player_id);

    auto players = players_.GetPlayersByMap(player->GetMapId());

    boost::json::object players_obj;

    for (const auto* p : players) {
        boost::json::object obj;

        obj["pos"] = {
            p->GetPosition().x,
            p->GetPosition().y
        };

        obj["speed"] = {
            p->GetSpeed().vx,
            p->GetSpeed().vy
        };

        obj["dir"] = DirectionToString(p->GetDirection());

        players_obj[std::to_string(p->GetId())] = obj;
    }

    boost::json::object result;
    result["players"] = players_obj;

    return result;
}

} // namespace app