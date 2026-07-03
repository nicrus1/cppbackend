#include "game_state.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

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

void GameState::CollectLootForDog(model::Dog& dog, const model::Map& map) {
    auto it = loot_managers_.find(map.GetId());
    if (it == loot_managers_.end()) {
        return;
    }
    
    auto& manager = it->second;
    auto items = manager->GetLootItems();
    auto dog_pos = dog.GetPosition();
    
    const double COLLECT_RADIUS = 0.5;
    
    std::vector<uint64_t> items_to_remove;
    
    for (const auto& [id, item] : items) {
        double dx = dog_pos.x - item.second.x;
        double dy = dog_pos.y - item.second.y;
        if (dx * dx + dy * dy < COLLECT_RADIUS * COLLECT_RADIUS) {
            // Собираем трофей
            // TODO: Получить стоимость трофея по его типу
            dog.AddScore(10); // Временное значение
            items_to_remove.push_back(id);
        }
    }
    
    for (auto id : items_to_remove) {
        manager->RemoveLootItem(id);
    }
}

void GameState::CheckDogInactivity(int64_t time_delta_ms) {
    auto now = std::chrono::steady_clock::now();
    std::vector<model::PlayerId> players_to_retire;
    std::vector<model::PlayerId> players_to_remove;
    
    for (auto& [player_id, dog] : dogs_) {
        model::Player* player = players_.FindPlayer(player_id);
        if (!player) continue;
        
        // Если собака движется - обновляем время активности
        auto speed = dog.GetSpeed();
        if (speed.vx != 0.0 || speed.vy != 0.0) {
            dog.SetLastActivityTime(now);
            continue;
        }
        
        // Проверяем, не превышено ли время бездействия
        auto idle_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - dog.GetLastActivityTime()
        );
        
        if (idle_time >= dog_retirement_time_) {
            // Собака уходит на пенсию
            RetireDog(dog, *player);
            players_to_retire.push_back(player_id);
            players_to_remove.push_back(player_id);
        }
    }
    
    // Удаляем собак и игроков, ушедших на пенсию
    for (auto player_id : players_to_retire) {
        // Удаляем собаку
        auto dog_it = dogs_.find(player_id);
        if (dog_it != dogs_.end()) {
            dogs_.erase(dog_it);
        }
    }
    
    // TODO: Удалить игроков из системы
    // Для этого нужно добавить метод RemovePlayer в Players
}

void GameState::RetireDog(model::Dog& dog, const model::Player& player) {
    if (record_manager_) {
        int score = dog.GetScore();
        double play_time = std::chrono::duration<double>(
            dog.GetTotalPlayTime()
        ).count();
        
        try {
            record_manager_->AddRecord(player.GetName(), score, play_time);
        } catch (const std::exception& e) {
            std::cerr << "Failed to save record: " << e.what() << std::endl;
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
    dog.SetRetirementTime(dog_retirement_time_);
    dog.SetLastActivityTime(std::chrono::steady_clock::now());
    
    dogs_.emplace(player.GetId(), std::move(dog));
    player.SetDogId(dog_id);

    // Создаем менеджер трофеев для карты если его нет
    auto it = loot_managers_.find(map_id);
    if (it == loot_managers_.end()) {
        auto manager = std::make_unique<LootManager>(
            *map, loot_period_, loot_probability_
        );
        loot_managers_[map_id] = std::move(manager);
        it = loot_managers_.find(map_id);
    }
    
    // Генерируем один трофей для нового игрока
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
    dog->SetLastActivityTime(std::chrono::steady_clock::now());
}

void GameState::StopDog(const model::Token& token) {
    model::Dog* dog = GetDogByTokenMutable(token);
    if (!dog) return;

    dog->SetSpeed({0.0, 0.0});
    // Время бездействия начинает отсчитываться с этого момента
    dog->SetLastActivityTime(std::chrono::steady_clock::now());
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
    
    // Обновляем общее время игры для всех собак
    for (auto& [player_id, dog] : dogs_) {
        auto total = dog.GetTotalPlayTime();
        dog.SetTotalPlayTime(total + delta);
    }
    
    // Двигаем собак и собираем трофеи
    for (auto& [player_id, dog] : dogs_) {
        model::Player* player = players_.FindPlayer(player_id);
        if (!player) continue;

        const model::Map* map = game_.FindMap(player->GetMapId());
        if (!map) continue;

        MoveDog(dog, *map, time_delta_ms);
        CollectLootForDog(dog, *map);
    }
    
    // Проверяем бездействие
    CheckDogInactivity(time_delta_ms);
    
    // Обновляем трофеи для каждой карты
    for (const auto& map : game_.GetMaps()) {
        auto it = loot_managers_.find(map.GetId());
        if (it == loot_managers_.end()) {
            auto manager = std::make_unique<LootManager>(
                map, loot_period_, loot_probability_
            );
            loot_managers_[map.GetId()] = std::move(manager);
            it = loot_managers_.find(map.GetId());
        }
        
        // Count dogs on this map
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

std::unordered_map<std::string, std::string> GameState::GetPlayersOnMap(const model::Token& token) const {
    const model::Player* player = players_.FindPlayerByToken(token);
    if (!player) {
        return {};
    }
    return GetPlayersOnMapForTest(player->GetMapId());
}

std::unordered_map<uint64_t, std::pair<int, model::Position>> 
GameState::GetLootState(const model::Token& token) const {
    const model::Player* player = players_.FindPlayerByToken(token);
    if (!player) {
        return {};
    }
    
    auto it = loot_managers_.find(player->GetMapId());
    if (it == loot_managers_.end()) {
        return {};
    }
    
    return it->second->GetLootItems();
}

} // namespace game