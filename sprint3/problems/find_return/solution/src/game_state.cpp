#include "game_state.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <limits>

namespace {

constexpr double ROAD_HALF_WIDTH = 0.4;
constexpr double DOG_HALF_WIDTH = 0.3;
constexpr double OFFICE_HALF_WIDTH = 0.25;
constexpr double LOOT_COLLISION_DIST = DOG_HALF_WIDTH;  // 0.3
constexpr double OFFICE_COLLISION_DIST = DOG_HALF_WIDTH + OFFICE_HALF_WIDTH;  // 0.55

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
    auto start_pos = dog.GetPosition();
    auto speed = dog.GetSpeed();

    if (speed.vx == 0 && speed.vy == 0) {
        return;
    }

    double target_x = start_pos.x + speed.vx * dt;
    double target_y = start_pos.y + speed.vy * dt;

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

        if (start_pos.x >= rx_min - EPSILON && start_pos.x <= rx_max + EPSILON &&
            start_pos.y >= ry_min - EPSILON && start_pos.y <= ry_max + EPSILON) {
            
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

    model::Position end_pos = start_pos;
    
    if (speed.vx != 0) {
        if (target_x < min_bound) {
            target_x = min_bound;
            dog.SetSpeed({0.0, 0.0});
        } else if (target_x > max_bound) {
            target_x = max_bound;
            dog.SetSpeed({0.0, 0.0});
        }
        end_pos = {target_x, start_pos.y};
        dog.SetPosition(target_x, start_pos.y);
    } else if (speed.vy != 0) {
        if (target_y < min_bound) {
            target_y = min_bound;
            dog.SetSpeed({0.0, 0.0});
        } else if (target_y > max_bound) {
            target_y = max_bound;
            dog.SetSpeed({0.0, 0.0});
        }
        end_pos = {start_pos.x, target_y};
        dog.SetPosition(start_pos.x, target_y);
    }
    
    // Обработка коллизий после перемещения
    // Находим ID собаки
    uint64_t dog_id = 0;
    for (const auto& [id, d] : dogs_) {
        if (&d == &dog) {
            dog_id = *id;
            break;
        }
    }
    
    if (dog_id != 0) {
        ProcessCollisions(dog, map, start_pos, end_pos, dog_id);
    }
}

bool GameState::CheckLootCollision(
    const model::Position& pos,
    const model::Position& loot_pos) {
    double dx = pos.x - loot_pos.x;
    double dy = pos.y - loot_pos.y;
    return (dx * dx + dy * dy) <= (LOOT_COLLISION_DIST * LOOT_COLLISION_DIST);
}

bool GameState::CheckOfficeCollision(
    const model::Position& pos,
    const model::Position& office_pos) {
    double dx = pos.x - office_pos.x;
    double dy = pos.y - office_pos.y;
    return (dx * dx + dy * dy) <= (OFFICE_COLLISION_DIST * OFFICE_COLLISION_DIST);
}

std::vector<std::pair<double, uint64_t>> GameState::FindLootCollisions(
    const model::Position& start,
    const model::Position& end,
    const std::unordered_map<uint64_t, std::pair<int, model::Position>>& loot_items,
    const std::unordered_map<uint64_t, model::Dog*>& dogs_on_map) {
    
    std::vector<std::pair<double, uint64_t>> collisions;
    
    // Проверяем каждый предмет
    for (const auto& [loot_id, loot_info] : loot_items) {
        const auto& loot_pos = loot_info.second;
        
        // Проверяем, находится ли предмет на отрезке пути
        // Используем параметрическое представление отрезка: P(t) = start + t * (end - start), t in [0, 1]
        double dx = end.x - start.x;
        double dy = end.y - start.y;
        double len_sq = dx * dx + dy * dy;
        
        if (len_sq < 1e-9) {
            // Если игрок не двигался, проверяем только начальную позицию
            if (CheckLootCollision(start, loot_pos)) {
                collisions.push_back({0.0, loot_id});
            }
            continue;
        }
        
        // Находим проекцию точки loot_pos на отрезок
        double t = ((loot_pos.x - start.x) * dx + (loot_pos.y - start.y) * dy) / len_sq;
        t = std::clamp(t, 0.0, 1.0);
        
        model::Position closest_point{
            start.x + t * dx,
            start.y + t * dy
        };
        
        // Проверяем коллизию с ближайшей точкой на отрезке
        if (CheckLootCollision(closest_point, loot_pos)) {
            collisions.push_back({t, loot_id});
        }
    }
    
    // Сортируем по времени достижения
    std::sort(collisions.begin(), collisions.end(), 
              [](const auto& a, const auto& b) { return a.first < b.first; });
    
    return collisions;
}

void GameState::ProcessCollisions(
    model::Dog& dog,
    const model::Map& map,
    const model::Position& start_pos,
    const model::Position& end_pos,
    uint64_t dog_id) {
    
    // Получаем менеджер лута для карты
    auto it = loot_managers_.find(map.GetId());
    if (it == loot_managers_.end()) {
        return;
    }
    
    auto& loot_manager = *it->second;
    auto loot_items = loot_manager.GetLootItems();
    
    // Создаем карту собак на этой карте для проверки конкуренции
    std::unordered_map<uint64_t, model::Dog*> dogs_on_map;
    for (auto& [id, d] : dogs_) {
        model::Player* player = players_.FindPlayer(id);
        if (player && player->GetMapId() == map.GetId()) {
            dogs_on_map[*id] = &d;
        }
    }
    
    // Находим коллизии с предметами
    auto collisions = FindLootCollisions(start_pos, end_pos, loot_items, dogs_on_map);
    
    // Обрабатываем коллизии в хронологическом порядке
    for (const auto& [t, loot_id] : collisions) {
        // Проверяем, существует ли еще предмет
        auto loot_it = loot_items.find(loot_id);
        if (loot_it == loot_items.end()) {
            continue;
        }
        
        // Проверяем, не полон ли рюкзак
        if (dog.IsBagFull()) {
            continue;
        }
        
        // Проверяем, не взял ли другой игрок этот предмет раньше
        bool taken_by_other = false;
        for (const auto& [other_id, other_dog] : dogs_on_map) {
            if (other_id == dog_id) continue;
            
            // Проверяем, достиг ли другой игрок предмета раньше
            // Для простоты проверяем, находится ли другой игрок ближе к предмету
            double dist_other = std::sqrt(
                std::pow(other_dog->GetPosition().x - loot_it->second.second.x, 2) +
                std::pow(other_dog->GetPosition().y - loot_it->second.second.y, 2)
            );
            
            if (dist_other <= LOOT_COLLISION_DIST) {
                taken_by_other = true;
                break;
            }
        }
        
        if (taken_by_other) {
            continue;
        }
        
        // Добавляем предмет в рюкзак
        dog.AddToBag(loot_id, loot_it->second.first);
        
        // Удаляем предмет с карты
        loot_manager.RemoveLootItem(loot_id);
        
        // Обновляем список предметов
        loot_items = loot_manager.GetLootItems();
    }
    
    // Проверяем коллизию с офисами (базами)
    for (const auto& office : map.GetOffices()) {
        model::Position office_pos = office.GetEntryPoint();
        
        // Проверяем, находится ли игрок рядом с офисом
        if (CheckOfficeCollision(end_pos, office_pos)) {
            // Сдаем все предметы на базу
            if (!dog.GetBag().empty()) {
                dog.ClearBag();
            }
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
    
    // Устанавливаем вместимость рюкзака для собаки
    dog.SetBagCapacity(map->GetBagCapacity());
    
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

void GameState::SetLootGeneratorConfig(double period, double probability) {
    loot_period_ = period;
    loot_probability_ = probability;
}

void GameState::ProcessTick(int64_t time_delta_ms) {
    auto delta = std::chrono::milliseconds(time_delta_ms);
    
    // Сначала обновляем лут для каждой карты
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
    
    // Затем двигаем собак и обрабатываем коллизии
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
        PlayerState state{
            std::to_string(*p->GetId()),
            d.GetPosition(),
            d.GetSpeed(),
            d.GetDirection(),
            d.GetBag()  // Добавляем содержимое рюкзака
        };
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