#include "game_state.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

namespace {

constexpr double ROAD_HALF_WIDTH = 0.4;

} // namespace

namespace game {

model::Position GameState::GenerateStartPosition(const model::Map& map) {
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

    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Алгоритм честного расчёта границ без раннего return
    if (speed.vx != 0) {
        double min_x = pos.x;
        double max_x = pos.x;
        bool on_road = false;

        for (const auto& road : map.GetRoads()) {
            if (road.IsHorizontal()) {
                double r_start_y = road.GetStart().y;
                // Проверяем, попадает ли текущий Y собаки в ширину дороги
                if (pos.y >= r_start_y - ROAD_HALF_WIDTH && pos.y <= r_start_y + ROAD_HALF_WIDTH) {
                    double r_min_x = std::min(road.GetStart().x, road.GetEnd().x) - ROAD_HALF_WIDTH;
                    double r_max_x = std::max(road.GetStart().x, road.GetEnd().x) + ROAD_HALF_WIDTH;
                    
                    // Если собака находится на этой дороге по оси X
                    if (pos.x >= r_min_x && pos.x <= r_max_x) {
                        if (!on_road) {
                            min_x = r_min_x;
                            max_x = r_max_x;
                            on_road = true;
                        } else {
                            min_x = std::min(min_x, r_min_x);
                            max_x = std::max(max_x, r_max_x);
                        }
                    }
                }
            }
        }

        if (on_road) {
            if (target_x < min_x) {
                target_x = min_x;
                dog.SetSpeed({0.0, 0.0});
            } else if (target_x > max_x) {
                target_x = max_x;
                dog.SetSpeed({0.0, 0.0});
            }
            dog.SetPosition(target_x, pos.y);
        } else {
            dog.SetSpeed({0.0, 0.0});
        }

    } else if (speed.vy != 0) {
        double min_y = pos.y;
        double max_y = pos.y;
        bool on_road = false;

        for (const auto& road : map.GetRoads()) {
            if (road.IsVertical()) {
                double r_start_x = road.GetStart().x;
                // Проверяем, попадает ли текущий X собаки в ширину дороги
                if (pos.x >= r_start_x - ROAD_HALF_WIDTH && pos.x <= r_start_x + ROAD_HALF_WIDTH) {
                    double r_min_y = std::min(road.GetStart().y, road.GetEnd().y) - ROAD_HALF_WIDTH;
                    double r_max_y = std::max(road.GetStart().y, road.GetEnd().y) + ROAD_HALF_WIDTH;
                    
                    // Если собака находится на этой дороге по оси Y
                    if (pos.y >= r_min_y && pos.y <= r_max_y) {
                        if (!on_road) {
                            min_y = r_min_y;
                            max_y = r_max_y;
                            on_road = true;
                        } else {
                            min_y = std::min(min_y, r_min_y);
                            max_y = std::max(max_y, r_max_y);
                        }
                    }
                }
            }
        }

        if (on_road) {
            if (target_y < min_y) {
                target_y = min_y;
                dog.SetSpeed({0.0, 0.0});
            } else if (target_y > max_y) {
                target_y = max_y;
                dog.SetSpeed({0.0, 0.0});
            }
            dog.SetPosition(pos.x, target_y);
        } else {
            dog.SetSpeed({0.0, 0.0});
        }
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
    
    model::Dog dog(dog_id, user_name);
    dog.SetPosition(start_pos.x, start_pos.y);
    
    dogs_.emplace(player.GetId(), std::move(dog));
    player.SetDogId(dog_id);

    return {token, player.GetId()};
}

const model::Dog* GameState::GetDogByToken(const model::Token& token) const {
    model::Player* player = players_.FindPlayerByToken(token);
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
    model::Player* player = players_.FindPlayerByToken(token);
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

    model::Player* player = players_.FindPlayerByToken(token);
    if (!player) return result;

    auto players_on_map = players_.GetPlayersOnMap(player->GetMapId());

    for (const auto* p : players_on_map) {
        auto it = dogs_.find(p->GetId());
        if (it == dogs_.end()) continue;

        const auto& d = it->second;
        result.push_back({
            std::to_string(*p->GetId()), // Ключ-строка ID игрока для JSON-карты
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