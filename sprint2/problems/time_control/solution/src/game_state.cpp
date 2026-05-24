#include "game_state.h"
#include "logger.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <stdexcept>

namespace game {

void GameState::BuildRoadMaps() {
    for (const auto& map : game_.GetMaps()) {
        model::RoadMap road_map;

        for (const auto& road : map.GetRoads()) {
            road_map.AddRoad(road);
        }

        road_maps_[map.GetId()] = std::move(road_map);
    }
}

model::Position GameState::GenerateStartPositionOnMap(const model::Map& map) {
    const auto& roads = map.GetRoads();

    if (roads.empty()) {
        return {0.0, 0.0};
    }

    const auto& road = roads.front();
    const auto start = road.GetStart();

    return {
        static_cast<double>(start.x),
        static_cast<double>(start.y)
    };
}

const model::RoadMap::RoadSegment* GameState::FindRoadForDog(
    const model::Dog& dog,
    const model::Map* map
) const {
    if (!map) {
        return nullptr;
    }

    auto it = road_maps_.find(map->GetId());
    if (it == road_maps_.end()) {
        return nullptr;
    }

    const auto& pos = dog.GetPosition();

    return it->second.FindRoad(pos.x, pos.y);
}

void GameState::UpdateDogPosition(
    model::Dog& dog,
    const model::Map* map,
    double delta_seconds
) {
    if (!map) {
        return;
    }

    const auto speed = dog.GetSpeed();

    if (std::abs(speed.vx) < 1e-9 && std::abs(speed.vy) < 1e-9) {
        return;
    }

    auto pos = dog.GetPosition();

    double new_x = pos.x + speed.vx * delta_seconds;
    double new_y = pos.y + speed.vy * delta_seconds;

    // Просто обновляем позицию без всяких проверок дорог
    dog.SetPosition({new_x, new_y});
    
    logger::LogDebug("UpdateDogPosition: pos=(" + std::to_string(new_x) + "," + 
                     std::to_string(new_y) + "), speed=(" + 
                     std::to_string(speed.vx) + "," + std::to_string(speed.vy) + 
                     "), delta=" + std::to_string(delta_seconds));
}

void GameState::UpdateTime(std::chrono::milliseconds delta) {
    const double delta_seconds = delta.count() / 1000.0;
    
    logger::LogDebug("UpdateTime: delta_ms=" + std::to_string(delta.count()) + 
                     ", delta_sec=" + std::to_string(delta_seconds));

    for (auto& [player_id, dog] : dogs_) {
        const model::Player* player = players_.FindPlayer(player_id);

        if (!player) {
            continue;
        }

        const model::Map* map = game_.FindMap(player->GetMapId());

        if (!map) {
            continue;
        }

        UpdateDogPosition(dog, map, delta_seconds);
    }
}

GameState::JoinResult GameState::JoinGame(
    const std::string& user_name,
    const model::Map::Id& map_id
) {
    const model::Map* map = game_.FindMap(map_id);

    if (!map) {
        throw std::runtime_error("Map not found");
    }

    model::Player& player = players_.AddPlayer(user_name, map_id);
    model::Token token = players_.GenerateToken(player);

    model::Position start_pos = GenerateStartPositionOnMap(*map);

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

    if (!player) {
        return nullptr;
    }

    auto it = dogs_.find(player->GetId());

    if (it == dogs_.end()) {
        return nullptr;
    }

    return &it->second;
}

model::Dog* GameState::GetDogByTokenMutable(const model::Token& token) {
    model::Player* player = players_.FindPlayerByToken(token);

    if (!player) {
        return nullptr;
    }

    auto it = dogs_.find(player->GetId());

    if (it == dogs_.end()) {
        return nullptr;
    }

    return &it->second;
}

const model::Map* GameState::GetPlayerMap(const model::Token& token) const {
    const model::Player* player = players_.FindPlayerByToken(token);

    if (!player) {
        return nullptr;
    }

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

    const double speed = map->GetDogSpeed();

    dog->SetSpeedFromDirection(dir, speed);
    
    logger::LogDebug("SetDogDirection: dir=" + model::DirectionToString(dir) + 
                     ", speed=" + std::to_string(speed) +
                     ", vx=" + std::to_string(dog->GetSpeed().vx) + 
                     ", vy=" + std::to_string(dog->GetSpeed().vy));
}

void GameState::StopDog(const model::Token& token) {
    model::Dog* dog = GetDogByTokenMutable(token);

    if (!dog) {
        throw std::runtime_error("Dog not found");
    }

    dog->Stop();
    
    logger::LogDebug("StopDog: dog stopped");
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

std::unordered_map<std::string, std::string> GameState::GetPlayersOnMapForTest(
    const model::Map::Id& map_id
) const {
    auto players_on_map = players_.GetPlayersOnMap(map_id);

    std::unordered_map<std::string, std::string> result;

    for (auto* p : players_on_map) {
        result[std::to_string(*p->GetId())] = p->GetName();
    }

    return result;
}

std::unordered_map<std::string, std::string> GameState::GetPlayersOnMap(
    const model::Token& token
) {
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

}  // namespace game