#include "game_state.h"

#include <random>
#include <stdexcept>

namespace game {

namespace {

model::Token GenerateToken() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());

    std::uniform_int_distribution<uint64_t> dist;

    std::stringstream ss;

    ss << std::hex
       << dist(gen)
       << dist(gen);

    return ss.str();
}

} // namespace

JoinResult GameState::JoinGame(const std::string& user_name,
                               const model::Map::Id& map_id) {

    const model::Map* map = game_.FindMap(map_id);

    if (!map) {
        throw std::runtime_error("Map not found");
    }

    app::Player& player = players_.AddPlayer(user_name, map_id);

    if (!map->GetRoads().empty()) {

        const auto& road = map->GetRoads().front();

        auto start = road.GetStart();

        player.SetPosition({
            start.x,
            start.y
        });
    }

    player.SetSpeed({0.0, 0.0});
    player.SetDirection(model::Direction::NORTH);

    model::Token token = GenerateToken();

    players_.AddPlayerToken(token, player);

    return {
        token,
        player.GetId()
    };
}

bool GameState::ValidateToken(const model::Token& token) const {
    return players_.FindPlayerByToken(token) != nullptr;
}

std::unordered_map<std::string, std::string>
GameState::GetPlayersOnMap(const model::Token& token) const {

    const app::Player* current_player =
        players_.FindPlayerByToken(token);

    if (!current_player) {
        throw std::runtime_error("Player not found");
    }

    auto players =
        players_.GetPlayersByMap(current_player->GetMapId());

    std::unordered_map<std::string, std::string> result;

    for (const auto* player : players) {
        result.emplace(
            std::to_string(player->GetId()),
            player->GetName()
        );
    }

    return result;
}

std::unordered_map<PlayerId, app::Player*>
GameState::GetGameState(const model::Token& token) {

    app::Player* current_player =
        players_.FindPlayerByToken(token);

    if (!current_player) {
        throw std::runtime_error("Player not found");
    }

    auto players =
        players_.GetPlayersByMap(current_player->GetMapId());

    std::unordered_map<PlayerId, app::Player*> result;

    for (auto* player : players) {
        result[player->GetId()] = player;
    }

    return result;
}

} // namespace game