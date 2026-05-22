#include "game_state.h"

#include <random>
#include <sstream>
#include <iomanip>

namespace game {

static app::Token GenerateToken() {
    static std::mt19937_64 generator{std::random_device{}()};

    std::uniform_int_distribution<uint64_t> dist;

    std::ostringstream out;

    out << std::hex
        << std::setw(16) << std::setfill('0') << dist(generator)
        << std::setw(16) << std::setfill('0') << dist(generator);

    return out.str();
}

JoinResult GameState::JoinGame(const std::string& user_name,
                               const model::Map::Id& map_id) {

    auto& player =
        players_.AddPlayer(user_name,
                           map_id,
                           static_cast<app::Player::Id>(next_player_id_++));

    app::Token token = GenerateToken();

    token_to_player_[token] = &player;

    return {player.GetId(), token};
}

bool GameState::ValidateToken(const app::Token& token) const {
    return token_to_player_.count(token) != 0;
}

std::unordered_map<std::string, std::string>
GameState::GetPlayersOnMap(const app::Token& token) const {

    std::unordered_map<std::string, std::string> result;

    auto it = token_to_player_.find(token);

    if (it == token_to_player_.end()) {
        return result;
    }

    auto* current_player = it->second;

    auto players =
        players_.GetPlayersByMap(current_player->GetMapId());

    for (auto* player : players) {
        result[std::to_string(player->GetId())] =
            player->GetName();
    }

    return result;
}

std::unordered_map<std::string, app::Player*>
GameState::GetGameState(const app::Token& token) {

    std::unordered_map<std::string, app::Player*> result;

    auto it = token_to_player_.find(token);

    if (it == token_to_player_.end()) {
        return result;
    }

    auto* current_player = it->second;

    auto players =
        players_.GetPlayersByMap(current_player->GetMapId());

    for (auto* player : players) {
        result[std::to_string(player->GetId())] = player;
    }

    return result;
}

}  // namespace game