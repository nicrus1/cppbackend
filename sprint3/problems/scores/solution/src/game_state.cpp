#include "game_state.h"
#include "collision.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <set>
#include <tuple>

namespace {

constexpr double ROAD_HALF_WIDTH = 0.4;

} // namespace

namespace game {

GameState::GameState(model::Game& game)
    : game_(game) {}

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

    double min_bound = 0.0;
    double max_bound = 0.0;
    bool on_road = false;

    const double EPSILON = 1e-4;

    for (const auto& road : map.GetRoads()) {
        double rx_min, rx_max, ry_min, ry_max;
        if (road.IsHorizontal()) {
            rx_min = std::min(road.GetStart().x, road.GetEnd().x) - ROAD_HALF_WIDTH;
            rx_max = std::max(road.GetStart().x, road.GetEnd().x) + ROAD_HALF_WIDTH;
            ry_min = road.GetStart().y - ROAD_HALF_WIDTH;
            ry_max = road.GetStart().y + ROAD_HALF_WIDTH;
        } else {
            rx_min = road.GetStart().x - ROAD_HALF_WIDTH;
            rx_max = road.GetStart().x + ROAD_HALF_WIDTH;
            ry_min = std::min(road.GetStart().y, road.GetEnd().y) - ROAD_HALF_WIDTH;
            ry_max = std::max(road.GetStart().y, road.GetEnd().y) + ROAD_HALF_WIDTH;
        }

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

    if (!on_road) {
        dog.SetSpeed({0.0, 0.0});
        return;
    }

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

void GameState::ProcessDogCollisions(model::Dog& dog, 
                                    model::Player& player,
                                    const model::Map& map,
                                    const model::Position& start_pos,
                                    const model::Position& end_pos) {
    auto it = loot_managers_.find(map.GetId());
    if (it == loot_managers_.end()) return;
    
    auto& manager = *it->second;
    
    auto loot_items = manager.GetLootItems();
    if (loot_items.empty()) return;
    
    auto events = collision::FindLootCollisions(start_pos, end_pos, loot_items);

    std::set<uint64_t> collected_loot_ids;
    
    for (const auto& event : events) {
        if (event.type == collision::CollisionEvent::LOOT_PICKUP) {
            if (collected_loot_ids.find(event.loot_id) != collected_loot_ids.end()) {
                continue;
            }
            
            auto loot_it = loot_items.find(event.loot_id);
            if (loot_it == loot_items.end()) continue;
            
            if (dog.IsBagFull()) {
                continue;
            }
            
            int loot_type = std::get<0>(loot_it->second);
            int loot_value = std::get<1>(loot_it->second);
            dog.AddBagItem(event.loot_id, loot_type, loot_value);
            
            manager.RemoveLootItem(event.loot_id);
            collected_loot_ids.insert(event.loot_id);
        }
    }
    
    for (const auto& office : map.GetOffices()) {
        if (collision::IsNearOffice(end_pos, office)) {
            int total_score = 0;
            for (const auto& item : dog.GetBag()) {
                total_score += item.value;
            }
            if (total_score > 0) {
                dog.AddScore(total_score);
            }
            dog.ClearBag();
            break;
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
    double dog_speed = map->GetDogSpeed();
    
    model::Dog dog(dog_id, start_pos, dog_speed);
    dog.SetBagCapacity(map->GetBagCapacity());
    dog.SetScore(0);
    
    dogs_.emplace(player.GetId(), std::move(dog));
    player.SetDogId(dog_id);

    auto it = loot_managers_.find(map_id);
    if (it == loot_managers_.end()) {
        auto manager = std::make_unique<LootManager>(
            *map, loot_period_, loot_probability_
        );
        loot_managers_[map_id] = std::move(manager);
        it = loot_managers_.find(map_id);
    }
    
    it->second->AddLootItems(1);

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

void GameState::SetLootGeneratorConfig(double period, double probability) {
    loot_period_ = period;
    loot_probability_ = probability;
}

void GameState::ProcessTick(int64_t time_delta_ms) {
    auto delta = std::chrono::milliseconds(time_delta_ms);
    
    for (auto& [player_id, dog] : dogs_) {
        model::Player* player = players_.FindPlayer(player_id);
        if (!player) continue;

        const model::Map* map = game_.FindMap(player->GetMapId());
        if (!map) continue;

        model::Position start_pos = dog.GetPosition();
        
        MoveDog(dog, *map, time_delta_ms);
        
        ProcessDogCollisions(dog, *player, *map, start_pos, dog.GetPosition());
    }
    
    for (const auto& map : game_.GetMaps()) {
        auto it = loot_managers_.find(map.GetId());
        if (it == loot_managers_.end()) {
            auto manager = std::make_unique<LootManager>(
                map, loot_period_, loot_probability_
            );
            loot_managers_[map.GetId()] = std::move(manager);
            it = loot_managers_.find(map.GetId());
        }
        
        size_t dog_count = 0;
        for (const auto& [player_id, dog] : dogs_) {
            model::Player* player = players_.FindPlayer(player_id);
            if (player && player->GetMapId() == map.GetId()) {
                dog_count++;
            }
        }
        
        it->second->Update(delta, dog_count);
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
        PlayerState state{
            std::to_string(*p->GetId()),
            d.GetPosition(),
            d.GetSpeed(),
            d.GetDirection(),
            d.GetScore()
        };
        
        for (const auto& item : d.GetBag()) {
            state.bag.push_back({item.id, item.type});
        }
        
        result.push_back(std::move(state));
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

std::unordered_map<std::string, std::string> GameState::GetPlayersOnMap(const model::Token& token) const {
    const model::Player* player = players_.FindPlayerByToken(token);
    if (!player) {
        return {};
    }
    return GetPlayersOnMapForTest(player->GetMapId());
}

std::unordered_map<uint64_t, std::tuple<int, int, model::Position>> 
GameState::GetLootState(const model::Token& token) const {
    const model::Player* player = players_.FindPlayerByToken(token);
    if (!player) {
        return {};
    }
    
    auto it = loot_managers_.find(player->GetMapId());
    return (it != loot_managers_.end()) 
        ? it->second->GetLootItems() 
        : std::unordered_map<uint64_t, std::tuple<int, int, model::Position>>{};
}

} // namespace game