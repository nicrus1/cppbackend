#include "game_state.h"

#include <random>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace game {

std::optional<model::Road> GameState::SelectFirstRoad(const model::Map& map) const {
    const auto& roads = map.GetRoads();

    if (roads.empty()) {
        return std::nullopt;
    }

    return roads[0];
}

model::Position GameState::GenerateStartPosition(const model::Map& map) {
    auto road_opt = SelectFirstRoad(map);

    if (!road_opt) {
        return {0.0, 0.0};
    }

    const auto& road = *road_opt;
    auto start = road.GetStart();

    return {
        static_cast<double>(start.x),
        static_cast<double>(start.y)
    };
}

const model::RoadMap::RoadSegment* GameState::FindRoadAt(
    const model::Map& map,
    double x,
    double y) const {

    auto it = road_maps_.find(map.GetId());

    if (it == road_maps_.end()) {
        model::RoadMap road_map;

        for (const auto& road : map.GetRoads()) {
            road_map.AddRoad(road);
        }

        it = road_maps_.emplace(map.GetId(), std::move(road_map)).first;
    }

    return it->second.FindRoad(x, y);
}

model::Position GameState::MoveDogWithCollision(
    const model::Dog& dog,
    const model::Map& map,
    double delta_seconds) const {

    const auto& pos = dog.GetPosition();
    const auto& speed = dog.GetSpeed();

    if (speed.vx == 0.0 && speed.vy == 0.0) {
        return pos;
    }

    // Ищем дорогу по ТЕКУЩЕЙ позиции, а не по новой.
    // Это позволяет корректно дойти до края дороги.
    const auto* road_seg = FindRoadAt(map, pos.x, pos.y);

    if (!road_seg) {
        return pos;
    }

    double new_x = pos.x;
    double new_y = pos.y;

    if (road_seg->road.IsHorizontal()) {
        new_x += speed.vx * delta_seconds;

        // Ограничиваем движение границами дороги
        new_x = std::clamp(
            new_x,
            road_seg->left,
            road_seg->right
        );

        // Y фиксирован для горизонтальной дороги
        new_y = road_seg->top;
    } else {
        new_y += speed.vy * delta_seconds;

        // Ограничиваем движение границами дороги
        new_y = std::clamp(
            new_y,
            road_seg->top,
            road_seg->bottom
        );

        // X фиксирован для вертикальной дороги
        new_x = road_seg->left;
    }

    return {new_x, new_y};
}

void GameState::UpdateDogPosition(
    model::Dog& dog,
    const model::Map& map,
    double delta_seconds) {

    model::Position new_pos =
        MoveDogWithCollision(dog, map, delta_seconds);

    dog.SetPosition(new_pos);
}

void GameState::ProcessTick(int64_t time_delta_ms) {
    if (time_delta_ms <= 0) {
        return;
    }

    double delta_seconds = time_delta_ms / 1000.0;

    for (auto& [player_id, dog] : dogs_) {
        const auto* player = players_.FindPlayer(player_id);

        if (!player) {
            continue;
        }

        const model::Map* map = game_.FindMap(player->GetMapId());

        if (!map) {
            continue;
        }

        auto old_pos = dog.GetPosition();

        UpdateDogPosition(dog, *map, delta_seconds);

        const auto& new_pos = dog.GetPosition();
        const auto& speed = dog.GetSpeed();

        // Если собака упёрлась в границу и не сдвинулась —
        // останавливаем её.
        if (speed.vx != 0.0 || speed.vy != 0.0) {
            if (std::abs(old_pos.x - new_pos.x) < 1e-9 &&
                std::abs(old_pos.y - new_pos.y) < 1e-9) {

                dog.Stop();
            }
        }
    }
}

GameState::JoinResult GameState::JoinGame(
    const std::string& user_name,
    const model::Map::Id& map_id) {

    const model::Map* map = game_.FindMap(map_id);

    if (!map) {
        throw std::runtime_error("Map not found");
    }

    model::Player& player =
        players_.AddPlayer(user_name, map_id);

    model::Token token =
        players_.GenerateToken(player);

    model::Position start_pos =
        GenerateStartPosition(*map);

    uint64_t dog_id = *player.GetId();

    model::Dog dog(dog_id, start_pos);

    dogs_.emplace(player.GetId(), std::move(dog));

    player.SetDogId(dog_id);

    JoinResult result;
    result.token = std::move(token);
    result.player_id = player.GetId();

    return result;
}

const model::Dog* GameState::GetDogByToken(
    const model::Token& token) const {

    const model::Player* player =
        players_.FindPlayerByToken(token);

    if (!player) {
        return nullptr;
    }

    auto it = dogs_.find(player->GetId());

    if (it == dogs_.end()) {
        return nullptr;
    }

    return &it->second;
}

model::Dog* GameState::GetDogByTokenMutable(
    const model::Token& token) {

    model::Player* player =
        players_.FindPlayerByToken(token);

    if (!player) {
        return nullptr;
    }

    auto it = dogs_.find(player->GetId());

    if (it == dogs_.end()) {
        return nullptr;
    }

    return &it->second;
}

const model::Map* GameState::GetPlayerMap(
    const model::Token& token) const {

    const model::Player* player =
        players_.FindPlayerByToken(token);

    if (!player) {
        return nullptr;
    }

    return game_.FindMap(player->GetMapId());
}

void GameState::SetDogDirection(
    const model::Token& token,
    model::Direction dir) {

    model::Dog* dog =
        GetDogByTokenMutable(token);

    if (!dog) {
        throw std::runtime_error("Dog not found");
    }

    const model::Map* map =
        GetPlayerMap(token);

    if (!map) {
        throw std::runtime_error("Map not found");
    }

    double speed = map->GetDogSpeed();

    dog->SetSpeedFromDirection(dir, speed);
}

void GameState::StopDog(const model::Token& token) {
    model::Dog* dog =
        GetDogByTokenMutable(token);

    if (!dog) {
        throw std::runtime_error("Dog not found");
    }

    dog->Stop();
}

std::vector<GameState::PlayerState>
GameState::GetGameState(const model::Token& token) const {

    const model::Player* player =
        players_.FindPlayerByToken(token);

    if (!player) {
        return {};
    }

    std::vector<PlayerState> result;

    auto players_on_map =
        players_.GetPlayersOnMap(player->GetMapId());

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

bool GameState::ValidateToken(
    const model::Token& token) const {

    return players_.ValidateToken(token);
}

std::unordered_map<std::string, std::string>
GameState::GetPlayersOnMapForTest(
    const model::Map::Id& map_id) const {

    auto players_on_map =
        players_.GetPlayersOnMap(map_id);

    std::unordered_map<std::string, std::string> result;

    for (auto* p : players_on_map) {
        result[std::to_string(*p->GetId())] =
            p->GetName();
    }

    return result;
}

std::unordered_map<std::string, std::string>
GameState::GetPlayersOnMap(
    const model::Token& token) {

    model::Player* player =
        players_.FindPlayerByToken(token);

    if (!player) {
        throw std::runtime_error(
            "Invalid token or player not found");
    }

    auto players_on_map =
        players_.GetPlayersOnMap(player->GetMapId());

    std::unordered_map<std::string, std::string> result;

    for (auto* p : players_on_map) {
        result[std::to_string(*p->GetId())] =
            p->GetName();
    }

    return result;
}

} // namespace game