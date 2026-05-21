#include "game_state.h"

namespace game {

GameState::JoinResult GameState::JoinGame(const std::string& user_name, const model::Map::Id& map_id) {
    const model::Map* map = game_.FindMap(map_id);
    if (!map) {
        throw std::runtime_error("Map not found");
    }
    
    model::Player& player = players_.AddPlayer(user_name, map_id);
    model::Token token = players_.GenerateToken(player);
    
    JoinResult result;
    result.token = std::move(token);
    result.player_id = player.GetId();
    return result;
}

std::unordered_map<std::string, std::string> GameState::GetPlayersOnMap(const model::Token& token) {
    model::Player* player = players_.FindPlayerByToken(token);
    if (!player) {
        throw std::runtime_error("Invalid token or player not found");
    }
    
    auto players_on_map = players_.GetPlayersOnMap(player->GetMapId());
    std::unordered_map<std::string, std::string> result;
    for (auto* p : players_on_map) {
        result[std::to_string(*p->GetId())] = p->GetName();
    }
    return result;
}

bool GameState::ValidateToken(const model::Token& token) const {
    return players_.ValidateToken(token);
}

std::unordered_map<std::string, std::string> GameState::GetPlayersOnMapForTest(const model::Map::Id& map_id) const {
    auto players_on_map = players_.GetPlayersOnMap(map_id);
    std::unordered_map<std::string, std::string> result;
    for (auto* p : players_on_map) {
        result[std::to_string(*p->GetId())] = p->GetName();
    }
    return result;
}

} // namespace game