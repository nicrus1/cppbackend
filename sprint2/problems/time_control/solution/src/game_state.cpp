#include "game_state.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

namespace game {

std::optional<model::Road> GameState::SelectFirstRoad(const model::Map& map) const {
    const auto& roads = map.GetRoads();

    if (roads.empty()) {
        return std::nullopt;
    }

    return roads.front();
}

model::Position GameState::GenerateStartPosition(const model::Map& map) {
    auto road_opt = SelectFirstRoad(map);

    if (!road_opt) {
        return {0.0, 0.0};
    }

    const auto start = road_opt->GetStart();

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

        it = road_maps_.emplace(
            map.GetId(),
            std::move(road_map)
        ).first;
    }

    return it->second.FindRoad(x, y);
}

bool GameState::CanMoveInDirection(
    const model::RoadMap::RoadSegment* road_seg,
    model::Direction dir) const {
    
    if (!road_seg) {
        return false;
    }
    
    // На горизонтальной дороге можно двигаться только влево/вправо
    if (road_seg->road.IsHorizontal()) {
        return dir == model::Direction::WEST || dir == model::Direction::EAST;
    }
    // На вертикальной дороге можно двигаться только вверх/вниз
    else {
        return dir == model::Direction::NORTH || dir == model::Direction::SOUTH;
    }
}

model::Position GameState::ClampPositionToRoad(
    const model::RoadMap::RoadSegment* road_seg,
    double x,
    double y) const {
    
    if (!road_seg) {
        return {x, y};
    }
    
    if (road_seg->road.IsHorizontal()) {
        // Горизонтальная дорога: x ограничен, y фиксирован
        x = std::clamp(x, road_seg->left, road_seg->right);
        y = std::clamp(y, road_seg->top - ROAD_HALF_WIDTH, road_seg->top + ROAD_HALF_WIDTH);
    } else {
        // Вертикальная дорога: y ограничен, x фиксирован
        x = std::clamp(x, road_seg->left - ROAD_HALF_WIDTH, road_seg->left + ROAD_HALF_WIDTH);
        y = std::clamp(y, road_seg->top, road_seg->bottom);
    }
    
    return {x, y};
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

    const auto* road_seg = FindRoadAt(map, pos.x, pos.y);

    if (!road_seg) {
        return pos;
    }
    
    // Проверяем, можно ли двигаться в текущем направлении
    if (!CanMoveInDirection(road_seg, dog.GetDirection())) {
        // Нельзя двигаться - останавливаем собаку (но это будет сделано в UpdateDogPosition)
        return pos;
    }

    double new_x = pos.x + speed.vx * delta_seconds;
    double new_y = pos.y + speed.vy * delta_seconds;
    
    // Ограничиваем позицию границами дороги
    auto clamped = ClampPositionToRoad(road_seg, new_x, new_y);
    
    return clamped;
}

void GameState::UpdateDogPosition(
    model::Dog& dog,
    const model::Map& map,
    double delta_seconds) {

    auto new_pos = MoveDogWithCollision(dog, map, delta_seconds);
    
    // Проверяем, изменилась ли позиция
    const auto& old_pos = dog.GetPosition();
    if (std::abs(new_pos.x - old_pos.x) < 1e-9 && std::abs(new_pos.y - old_pos.y) < 1e-9) {
        // Не сдвинулись - останавливаем
        dog.Stop();
        return;
    }
    
    dog.SetPosition(new_pos);

    const auto* road_seg = FindRoadAt(map, new_pos.x, new_pos.y);
    
    if (!road_seg) {
        dog.Stop();
        return;
    }
    
    // Проверяем, достигли ли края дороги
    const auto& speed = dog.GetSpeed();
    
    if (road_seg->road.IsHorizontal()) {
        if ((speed.vx > 0.0 && new_pos.x >= road_seg->right - 1e-9) ||
            (speed.vx < 0.0 && new_pos.x <= road_seg->left + 1e-9)) {
            dog.Stop();
        }
    } else {
        if ((speed.vy > 0.0 && new_pos.y >= road_seg->bottom - 1e-9) ||
            (speed.vy < 0.0 && new_pos.y <= road_seg->top + 1e-9)) {
            dog.Stop();
        }
    }
}

void GameState::ProcessTick(int64_t time_delta_ms) {
    if (time_delta_ms <= 0) {
        return;
    }

    const double delta_seconds = static_cast<double>(time_delta_ms) / 1000.0;

    for (auto& [player_id, dog] : dogs_) {
        const auto* player = players_.FindPlayer(player_id);

        if (!player) {
            continue;
        }

        const model::Map* map = game_.FindMap(player->GetMapId());

        if (!map) {
            continue;
        }

        UpdateDogPosition(dog, *map, delta_seconds);
    }
}

GameState::JoinResult GameState::JoinGame(
    const std::string& user_name,
    const model::Map::Id& map_id) {

    const model::Map* map = game_.FindMap(map_id);

    if (!map) {
        throw std::runtime_error("Map not found");
    }

    model::Player& player = players_.AddPlayer(user_name, map_id);

    model::Token token = players_.GenerateToken(player);

    model::Position start_pos = GenerateStartPosition(*map);

    uint64_t dog_id = *player.GetId();

    model::Dog dog(dog_id, start_pos);
    
    // Устанавливаем начальную позицию на дороге
    const auto* road_seg = FindRoadAt(*map, start_pos.x, start_pos.y);
    if (road_seg) {
        auto clamped = ClampPositionToRoad(road_seg, start_pos.x, start_pos.y);
        dog.SetPosition(clamped);
    }

    dogs_.emplace(player.GetId(), std::move(dog));

    player.SetDogId(dog_id);

    JoinResult result;
    result.token = std::move(token);
    result.player_id = player.GetId();

    return result;
}

const model::Dog* GameState::GetDogByToken(
    const model::Token& token) const {

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

model::Dog* GameState::GetDogByTokenMutable(
    const model::Token& token) {

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

const model::Map* GameState::GetPlayerMap(
    const model::Token& token) const {

    const model::Player* player = players_.FindPlayerByToken(token);

    if (!player) {
        return nullptr;
    }

    return game_.FindMap(player->GetMapId());
}

void GameState::SetDogDirection(
    const model::Token& token,
    model::Direction dir) {

    model::Dog* dog = GetDogByTokenMutable(token);

    if (!dog) {
        throw std::runtime_error("Dog not found");
    }

    const model::Map* map = GetPlayerMap(token);

    if (!map) {
        throw std::runtime_error("Map not found");
    }
    
    // Проверяем, можно ли двигаться в этом направлении на текущей дороге
    const auto* road_seg = FindRoadAt(*map, dog->GetPosition().x, dog->GetPosition().y);
    
    if (!CanMoveInDirection(road_seg, dir)) {
        // Нельзя двигаться в этом направлении - просто останавливаем
        dog->Stop();
        return;
    }

    const double speed = map->GetDogSpeed();

    dog->SetSpeedFromDirection(dir, speed);
}

void GameState::StopDog(
    const model::Token& token) {

    model::Dog* dog = GetDogByTokenMutable(token);

    if (!dog) {
        throw std::runtime_error("Dog not found");
    }

    dog->Stop();
}

std::vector<GameState::PlayerState>
GameState::GetGameState(
    const model::Token& token) const {

    const model::Player* player = players_.FindPlayerByToken(token);

    if (!player) {
        return {};
    }

    std::vector<PlayerState> result;

    auto players_on_map = players_.GetPlayersOnMap(player->GetMapId());

    for (const auto* p : players_on_map) {
        auto it = dogs_.find(p->GetId());

        if (it == dogs_.end()) {
            continue;
        }

        const auto& d = it->second;

        result.push_back({
            std::to_string(*p->GetId()),
            d.GetPosition(),
            d.GetSpeed(),
            d.GetDirection()
        });
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

    auto players_on_map = players_.GetPlayersOnMap(map_id);

    std::unordered_map<std::string, std::string> result;

    for (auto* p : players_on_map) {
        result[std::to_string(*p->GetId())] = p->GetName();
    }

    return result;
}

std::unordered_map<std::string, std::string>
GameState::GetPlayersOnMap(
    const model::Token& token) {

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