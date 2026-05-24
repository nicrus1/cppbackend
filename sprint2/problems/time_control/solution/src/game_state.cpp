#include "game_state.h"
    return game_.FindMap(player->GetMapId());
}

void GameState::SetDogDirection(const model::Token& token, model::Direction dir) {
    model::Dog* dog = GetDogByTokenMutable(token);

    if (!dog) {
        throw std::runtime_error("Dog not found");
    }

    const model::Map* map = GetPlayerMap(token);

    if (!map) {
        throw std::runtime_error("Map not found");
    }

    double speed = map->GetDogSpeed();
    dog->SetSpeedFromDirection(dir, speed);
}

void GameState::StopDog(const model::Token& token) {
    model::Dog* dog = GetDogByTokenMutable(token);

    if (!dog) {
        throw std::runtime_error("Dog not found");
    }

    dog->Stop();
}

std::vector<GameState::PlayerState> GameState::GetGameState(const model::Token& token) const {
    const model::Player* player = players_.FindPlayerByToken(token);

    if (!player) {
        return {};
    }

    std::vector<PlayerState> result;
    auto players_on_map = players_.GetPlayersOnMap(player->GetMapId());

    for (const auto* p : players_on_map) {
        auto it = dogs_.find(p->GetId());

        if (it != dogs_.end()) {
            const auto& d = it->second;

            result.push_back({
                std::to_string(*p->GetId()),
                d.GetPosition(),
                d.GetSpeed(),
                d.GetDirection()
            });
        }
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

} // namespace game