#include "game_state.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <chrono>

namespace {

constexpr double ROAD_HALF_WIDTH = 0.4;

} // namespace

namespace game {

model::Position GameState::GenerateRandomStartPosition(const model::Map& map) {
    const auto& roads = map.GetRoads();
    if (roads.empty()) {
        return {0.0, 0.0};
    }
    
    // Выбираем случайную дорогу
    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    const auto& road = roads[road_dist(rng_)];
    
    const auto& start = road.GetStart();
    const auto& end = road.GetEnd();
    
    std::uniform_real_distribution<double> pos_dist(0.0, 1.0);
    
    if (road.IsHorizontal()) {
        double t = pos_dist(rng_);
        double x = start.x + t * (end.x - start.x);
        return {x, static_cast<double>(start.y)};
    } else {
        double t = pos_dist(rng_);
        double y = start.y + t * (end.y - start.y);
        return {static_cast<double>(start.x), y};
    }
}

model::Position GameState::GenerateStartPosition(const model::Map& map) {
    if (randomize_spawn_) {
        return GenerateRandomStartPosition(map);
    }
    
    const auto& roads = map.GetRoads();
    if (roads.empty()) {
        return {0.0, 0.0};
    }
    
    const auto& first_road = roads.front();
    auto start = first_road.GetStart();
    
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

    double min_bound = 0.0;
    double max_bound = 0.0;
    bool on_road = false;

    // Небольшая погрешность (эпсилон) для избежания багов с плавающей точкой
    const double EPSILON = 1e-4;

    for (const auto& road : map.GetRoads()) {
        // Вычисляем полные прямоугольные границы для любой дороги
        double rx_min, rx_max, ry_min, ry_max;
        if (road.IsHorizontal()) {
            rx_min = std::min(road.GetStart().x, road.GetEnd().x) - ROAD_HALF_WIDTH;
            rx_max = std::max(road.GetStart().x, road.GetEnd().x) + ROAD_HALF_WIDTH;
            ry_min = road.GetStart().y - ROAD_HALF_WIDTH;
            ry_max = road.GetStart().y + ROAD_HALF_WIDTH;
        } else { // IsVertical
            rx_min = road.GetStart().x - ROAD_HALF_WIDTH;
            rx_max = road.GetStart().x + ROAD_HALF_WIDTH;
            ry_min = std::min(road.GetStart().y, road.GetEnd().y) - ROAD_HALF_WIDTH;
            ry_max = std::max(road.GetStart().y, road.GetEnd().y) + ROAD_HALF_WIDTH;
        }

        // Проверяем, находится ли собака в границах этого прямоугольника
        if (pos.x >= rx_min - EPSILON && pos.x <= rx_max + EPSILON &&
            pos.y >= ry_min - EPSILON && pos.y <= ry_max + EPSILON) {
            
            if (speed.vx != 0) {
                min_bound = on_road ? std::min(min_bound, rx_min) : rx_min;
                max_bound = on_road ? std::max(max_bound, rx_max) : rx_max;
            } else if (speed.vy != 0) {
                min_bound = on_road ? std::min(min_bound, ry_min) : ry_min;
                max_bound = on_road ? std::max(max_bound, ry_max) : ry_max;
            }
            on_road = true;
        }
    }

    // Если собака чудесным образом вообще не на дороге
    if (!on_road) {
        dog.SetSpeed({0.0, 0.0});
        return;
    }

    // Применяем движение с отсечением по границам собранных дорог
    if (speed.vx != 0) {
        if (target_x < min_bound) {
            target_x = min_bound;
            dog.SetSpeed({0.0, 0.0});
        } else if (target_x > max_bound) {
            target_x = max_bound;
            dog.SetSpeed({0.0, 0.0});
        }
        dog.SetPosition(target_x, pos.y);
    } else if (speed.vy != 0) {
        if (target_y < min_bound) {
            target_y = min_bound;
            dog.SetSpeed({0.0, 0.0});
        } else if (target_y > max_bound) {
            target_y = max_bound;
            dog.SetSpeed({0.0, 0.0});
        }
        dog.SetPosition(pos.x, target_y);
    }
}

GameState::JoinResult GameState::JoinGame(const std::string& user_name, const model::Map::Id& map_id) {
    const model::Map* map = game_.FindMap(map_id);
    if (!map) {
        throw std::invalid_argument("Map not found");
    }

    model::Player& player = players_.AddPlayer(user_name, map_id);
    model::Token token = players_.GenerateToken(player);

    model::Position start_pos = GenerateStartPosition(*map);
    uint64_t dog_id = *player.GetId();
    double dog_speed = map->GetDogSpeed();
    
    model::Dog dog(dog_id, start_pos, dog_speed);
    
    dogs_.emplace(player.GetId(), std::move(dog));
    player.SetDogId(dog_id);

    return {token, player.GetId()};
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

void GameState::SetDogDirection(const model::Token& token, model::Direction dir) {
    model::Dog* dog = GetDogByTokenMutable(token);
    if (!dog) return;

    const model::Map* map = GetPlayerMap(token);
    if (!map) return;

    double speed = map->GetDogSpeed();

    switch (dir) {
        case model::Direction::NORTH: dog->SetSpeed({0.0, -speed}); break;
        case model::Direction::SOUTH: dog->SetSpeed({0.0, speed}); break;
        case model::Direction::WEST:  dog->SetSpeed({-speed, 0.0}); break;
        case model::Direction::EAST:  dog->SetSpeed({speed, 0.0}); break;
    }
    dog->SetDirection(dir);
}

void GameState::StopDog(const model::Token& token) {
    model::Dog* dog = GetDogByTokenMutable(token);
    if (!dog) return;

    dog->SetSpeed({0.0, 0.0});
}

const model::Map* GameState::GetPlayerMap(const model::Token& token) const {
    const model::Player* player = players_.FindPlayerByToken(token);
    if (!player) return nullptr;

    return game_.FindMap(player->GetMapId());
}

void GameState::ProcessTick(int64_t time_delta_ms) {
    for (auto& [player_id, dog] : dogs_) {
        model::Player* player = players_.FindPlayer(player_id);
        if (!player) continue;

        const model::Map* map = game_.FindMap(player->GetMapId());
        if (!map) continue;

        MoveDog(dog, *map, time_delta_ms);
    }
}

std::vector<GameState::PlayerState> GameState::GetGameState(const model::Token& token) const {
    std::vector<PlayerState> result;

    const model::Player* player = players_.FindPlayerByToken(token);
    if (!player) return result;

    auto players_on_map = players_.GetPlayersOnMap(player->GetMapId());

    for (const auto* p : players_on_map) {
        auto it = dogs_.find(p->GetId());
        if (it == dogs_.end()) continue;

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
        return {};
    }
    return GetPlayersOnMapForTest(player->GetMapId());
}

} // namespace game