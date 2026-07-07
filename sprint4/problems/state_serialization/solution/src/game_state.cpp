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
            dog.AddScore(10);
            items_to_remove.push_back(id);
        }
    }
    
    for (auto id : items_to_remove) {
        manager->RemoveLootItem(id);
    }
}

void GameState::CheckDogInactivity(int64_t time_delta_ms) {
    std::vector<model::PlayerId> players_to_remove;
    
    // Обновляем время бездействия для всех собак
    for (auto& [player_id, dog] : dogs_) {
        model::Player* player = players_.FindPlayer(player_id);
        if (!player) continue;
        
        auto speed = dog.GetSpeed();
        
        // Если собака движется, сбрасываем время бездействия
        if (speed.vx != 0.0 || speed.vy != 0.0) {
            idle_time_[player_id] = std::chrono::milliseconds(0);
            continue;
        }
        
        // Иначе увеличиваем время бездействия
        idle_time_[player_id] += std::chrono::milliseconds(time_delta_ms);
        
        // Проверяем, не превысило ли время бездействия лимит
        if (idle_time_[player_id] >= dog_retirement_time_) {
            RetireDog(dog, *player);
            players_to_remove.push_back(player_id);
        }
    }
    
    // Удаляем игроков, чьи собаки ушли на покой
    for (auto player_id : players_to_remove) {
        // Удаляем время бездействия
        idle_time_.erase(player_id);
        
        // Удаляем собаку
        auto dog_it = dogs_.find(player_id);
        if (dog_it != dogs_.end()) {
            dogs_.erase(dog_it);
        }
        // Удаляем игрока
        players_.RemovePlayer(player_id);
    }
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
    
    dogs_.emplace(player.GetId(), std::move(dog));
    player.SetDogId(dog_id);
    
    // Инициализируем время бездействия
    idle_time_[player.GetId()] = std::chrono::milliseconds(0);

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
    
    // Сбрасываем время бездействия при начале движения
    model::Player* player = players_.FindPlayerByToken(token);
    if (player) {
        idle_time_[player->GetId()] = std::chrono::milliseconds(0);
    }
}

void GameState::StopDog(const model::Token& token) {
    model::Dog* dog = GetDogByTokenMutable(token);
    if (!dog) return;

    dog->SetSpeed({0.0, 0.0});
    // Не сбрасываем время бездействия при остановке - оно начинает отсчитываться
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
    
    // Обновляем игровое время
    game_time_ms_ += time_delta_ms;
    
    // Обновляем время игры для всех собак
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
    
    // Обновляем генерацию трофеев
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
        result.push_back({
            std::to_string(*p->GetId()),
            d.GetPosition(),
            d.GetSpeed(),
            d.GetDirection(),
            d.GetScore()
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

void GameState::SaveState(serialization::GameState& state) const {
    // Сохраняем игровое время
    state.game_time_ms = game_time_ms_;
    
    // Сохраняем собак
    for (const auto& [player_id, dog] : dogs_) {
        serialization::DogRepr repr(dog);
        state.dogs.push_back(repr);
        
        // Находим карту игрока
        const model::Player* player = players_.FindPlayer(player_id);
        if (player) {
            state.dog_to_map[*dog.GetId()] = *player->GetMapId();
        }
    }
    
    // Сохраняем предметы
    for (const auto& [map_id, manager] : loot_managers_) {
        auto items = manager->GetLootItems();
        for (const auto& [id, item] : items) {
            serialization::LootItemRepr repr;
            repr.id = id;
            repr.type = item.first;
            repr.pos = item.second;
            state.loot_items.push_back(repr);
            state.loot_to_map[id] = *map_id;
        }
        
        // Сохраняем состояние менеджера
        serialization::GameState::LootManagerState mgr_state;
        mgr_state.map_id = *map_id;
        mgr_state.next_id = manager->GetNextId();
        mgr_state.time_without_loot = manager->GetTimeWithoutLoot();
        state.loot_managers.push_back(mgr_state);
    }
    
    // Сохраняем игроков
    for (const auto& [player_id, player_ptr] : players_.GetAllPlayers()) {
        const model::Player* player = player_ptr.get();
        if (!player) continue;
        
        serialization::PlayerRepr repr;
        repr.name = player->GetName();
        repr.map_id = *player->GetMapId();
        repr.dog_id = player->GetDogId();
        
        // Находим токен
        auto token = players_.FindTokenByPlayerId(player_id);
        if (token) {
            repr.token = **token;
        }
        
        state.players.push_back(repr);
    }
    
    // Сохраняем время бездействия
    for (const auto& [player_id, idle] : idle_time_) {
        state.idle_time[static_cast<uint32_t>(*player_id)] = idle.count();
    }
}

void GameState::RestoreState(const serialization::GameState& state) {
    // Очищаем текущее состояние
    dogs_.clear();
    loot_managers_.clear();
    idle_time_.clear();
    players_ = model::Players(); // Создаем нового менеджера игроков
    
    // Восстанавливаем игровое время
    game_time_ms_ = state.game_time_ms;
    
    // Восстанавливаем собак - сначала создаем временное хранилище
    std::unordered_map<uint32_t, model::Dog> temp_dogs;
    for (const auto& dog_repr : state.dogs) {
        model::Dog dog = dog_repr.Restore();
        temp_dogs[dog_repr.GetId()] = std::move(dog);
    }
    
    // Восстанавливаем игроков
    for (const auto& player_repr : state.players) {
        model::Map::Id map_id{player_repr.map_id};
        const model::Map* map = game_.FindMap(map_id);
        if (!map) continue;
        
        // Создаем игрока
        model::Player& player = players_.AddPlayer(player_repr.name, map_id);
        player.SetDogId(player_repr.dog_id);
        
        // Восстанавливаем токен
        if (!player_repr.token.empty()) {
            model::Token token{player_repr.token};
            players_.AddToken(player, token);
        }
        
        // Восстанавливаем собаку из временного хранилища
        auto it = temp_dogs.find(player_repr.dog_id);
        if (it != temp_dogs.end()) {
            dogs_.emplace(player.GetId(), std::move(it->second));
        }
    }
    
    // Восстанавливаем время бездействия
    for (const auto& [player_id, idle_ms] : state.idle_time) {
        idle_time_[model::PlayerId{player_id}] = std::chrono::milliseconds(idle_ms);
    }
    
    // Восстанавливаем предметы
    for (const auto& item_repr : state.loot_items) {
        auto it = state.loot_to_map.find(item_repr.id);
        if (it == state.loot_to_map.end()) continue;
        
        model::Map::Id map_id{it->second};
        auto mgr_it = loot_managers_.find(map_id);
        if (mgr_it == loot_managers_.end()) {
            const model::Map* map = game_.FindMap(map_id);
            if (!map) continue;
            
            auto manager = std::make_unique<LootManager>(*map, loot_period_, loot_probability_);
            mgr_it = loot_managers_.emplace(map_id, std::move(manager)).first;
        }
        
        mgr_it->second->AddLootItem(item_repr.id, item_repr.type, item_repr.pos);
    }
    
    // Восстанавливаем состояние менеджеров
    for (const auto& mgr_state : state.loot_managers) {
        model::Map::Id map_id{mgr_state.map_id};
        auto it = loot_managers_.find(map_id);
        if (it != loot_managers_.end()) {
            it->second->SetNextId(mgr_state.next_id);
            it->second->SetTimeWithoutLoot(mgr_state.time_without_loot);
        }
    }
}

} // namespace game