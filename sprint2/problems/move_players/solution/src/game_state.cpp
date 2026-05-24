#include "game_state.h"
#include <random>
#include <cmath>

namespace game {

std::optional<model::Road> GameState::SelectRandomRoad(const model::Map& map) const {
    const auto& roads = map.GetRoads();
    if (roads.empty()) return std::nullopt;
    std::uniform_int_distribution<size_t> dist(0, roads.size() - 1);
    return roads[dist(rng_)];
}

model::Position GameState::GenerateRandomPositionOnMap(const model::Map& map) {
    auto road_opt = SelectRandomRoad(map);
    if (!road_opt) return {0.0, 0.0};

    const auto& road = *road_opt;
    auto start = road.GetStart();
    auto end = road.GetEnd();

    std::uniform_real_distribution<double> dist_x(std::min(start.x, end.x), std::max(start.x, end.x));
    std::uniform_real_distribution<double> dist_y(std::min(start.y, end.y), std::max(start.y, end.y));

    if (road.IsHorizontal()) {
        return {dist_x(rng_), static_cast<double>(start.y)};
    } else {
        return {static_cast<double>(start.x), dist_y(rng_)};
    }
}

GameState::JoinResult GameState::JoinGame(const std::string& user_name, const model::Map::Id& map_id) {
    const model::Map* map = game_.FindMap(map_id);
    if (!map) throw std::runtime_error("Map not found");

    model::Player& player = players_.AddPlayer(user_name, map_id);
    model::Token token = players_.GenerateToken(player);

    // Генерируем собаку
    model::Position start_pos = GenerateRandomPositionOnMap(*map);
    uint64_t dog_id = *player.GetId();
    model::Dog dog(dog_id, start_pos);
    dogs_.emplace(player.GetId(), std::move(dog));
    player.SetDogId(dog_id);

    JoinResult result;
    result.token = std::move(token);
    result.player_id = player.GetId();
    return result;
}

const model::Dog* GameState::GetDogByToken(const model::Token& token) const {
    const model::Player* player = players_.FindPlayerByToken(token);
    if (!player) return nullptr;
    auto it = dogs_.find(player->GetId());
    if (it == dogs_.end()) return nullptr;
    return &it->second;
}

model::Dog* GameState::GetDogByTokenMutable(const model::Token& token) {
    model::Player* player = players_.FindPlayerByToken(token);
    if (!player) return nullptr;
    auto it = dogs_.find(player->GetId());
    if (it == dogs_.end()) return nullptr;
    return &it->second;
}

const model::Map* GameState::GetPlayerMap(const model::Token& token) const {
    const model::Player* player = players_.FindPlayerByToken(token);
    if (!player) return nullptr;
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
    if (!player) return {};

    const model::Dog* dog = GetDogByToken(token);
    if (!dog) return {};

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