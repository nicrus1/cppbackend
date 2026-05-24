#include "game_state.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

namespace {

constexpr double ROAD_HALF_WIDTH = 0.4;

bool IsHorizontal(const model::Road& road) {
    return road.IsHorizontal();
}

bool IsVertical(const model::Road& road) {
    return road.IsVertical();
}

} // namespace

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

void GameState::MoveDog(model::Dog& dog, const model::Map& map, int64_t time_delta_ms) {
    const double dt = time_delta_ms / 1000.0;

    auto pos = dog.GetPosition();
    auto speed = dog.GetSpeed();

    if (speed.vx == 0 && speed.vy == 0) {
        return;
    }

    double target_x = pos.x + speed.vx * dt;
    double target_y = pos.y + speed.vy * dt;

    bool moved = false;

    for (const auto& road : map.GetRoads()) {
        if (road.IsHorizontal()) {
            auto start = road.GetStart();
            auto end = road.GetEnd();

            double min_x = std::min(start.x, end.x) - ROAD_HALF_WIDTH;
            double max_x = std::max(start.x, end.x) + ROAD_HALF_WIDTH;

            double road_y = start.y;

            if (std::abs(pos.y - road_y) > ROAD_HALF_WIDTH) {
                continue;
            }

            if (target_x < min_x) {
                target_x = min_x;
                speed.vx = 0;
                speed.vy = 0;
            }

            if (target_x > max_x) {
                target_x = max_x;
                speed.vx = 0;
                speed.vy = 0;
            }

            // Используем SetPosition с двумя координатами
            dog.SetPosition(target_x, road_y);
            dog.SetSpeed(speed);

            moved = true;
            break;
        }

        if (road.IsVertical()) {
            auto start = road.GetStart();
            auto end = road.GetEnd();

            double min_y = std::min(start.y, end.y) - ROAD_HALF_WIDTH;
            double max_y = std::max(start.y, end.y) + ROAD_HALF_WIDTH;

            double road_x = start.x;

            if (std::abs(pos.x - road_x) > ROAD_HALF_WIDTH) {
                continue;
            }

            if (target_y < min_y) {
                target_y = min_y;
                speed.vx = 0;
                speed.vy = 0;
            }

            if (target_y > max_y) {
                target_y = max_y;
                speed.vx = 0;
                speed.vy = 0;
            }

            // Используем SetPosition с двумя координатами
            dog.SetPosition(road_x, target_y);
            dog.SetSpeed(speed);

            moved = true;
            break;
        }
    }

    if (!moved) {
        dog.SetSpeed({0.0, 0.0});
    }
}

void GameState::ProcessTick(int64_t time_delta_ms) {
    if (time_delta_ms <= 0) {
        return;
    }

    for (auto& [player_id, dog] : dogs_) {
        const auto* player = players_.FindPlayer(player_id);

        if (!player) {
            continue;
        }

        const model::Map* map = game_.FindMap(player->GetMapId());

        if (!map) {
            continue;
        }

        MoveDog(dog, *map, time_delta_ms);
    }
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

    dog->SetDirection(dir);

    const double speed = dog->GetDefaultSpeed();

    switch (dir) {
        case model::Direction::NORTH:
            dog->SetSpeed({0.0, -speed});
            break;
        case model::Direction::SOUTH:
            dog->SetSpeed({0.0, speed});
            break;
        case model::Direction::WEST:
            dog->SetSpeed({-speed, 0.0});
            break;
        case model::Direction::EAST:
            dog->SetSpeed({speed, 0.0});
            break;
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

    double dog_speed = map->GetDogSpeed();

    model::Dog dog(dog_id, start_pos, dog_speed);

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

void GameState::StopDog(
    const model::Token& token) {

    model::Dog* dog = GetDogByTokenMutable(token);

    if (!dog) {
        throw std::runtime_error("Dog not found");
    }

    dog->SetSpeed({0.0, 0.0});
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