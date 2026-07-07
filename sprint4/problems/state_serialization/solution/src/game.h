#pragma once

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <fstream>

#include "model.h"
#include "app_listener.h"
#include "model_serialization.h"

namespace model {

// Структура для хранения информации об игроке
struct PlayerInfo {
    std::string token;
    std::string user_id;
    std::shared_ptr<Dog> dog;
    std::string map_id;
};

// Структура для хранения предмета на карте
struct LootItem {
    uint32_t id;
    uint32_t type;
    geom::Point2D position;
    bool is_collected = false;
};

// Структура для хранения состояния карты
struct MapState {
    std::string map_id;
    std::vector<std::shared_ptr<Dog>> dogs;
    std::vector<LootItem> loot_items;
    uint64_t last_loot_generation_time = 0;
};

class Game {
public:
    using ListenerPtr = std::shared_ptr<app::ApplicationListener>;
    
    Game() = default;
    
    void AddListener(ListenerPtr listener) {
        listeners_.push_back(listener);
    }
    
    void Tick(app::milliseconds delta) {
        game_time_ms_ += delta.count();
        
        for (auto& map_pair : maps_) {
            UpdateMap(map_pair.second, delta);
        }
        
        for (auto& listener : listeners_) {
            listener->OnTick(delta);
        }
    }
    
    void Shutdown() {
        for (auto& listener : listeners_) {
            listener->OnShutdown();
        }
    }
    
    // Методы для работы с собаками
    void AddDog(const std::string& map_id, std::shared_ptr<Dog> dog) {
        auto it = maps_.find(map_id);
        if (it == maps_.end()) {
            throw std::runtime_error("Map not found: " + map_id);
        }
        it->second.dogs.push_back(dog);
    }
    
    std::vector<std::shared_ptr<Dog>> GetDogs(const std::string& map_id) const {
        auto it = maps_.find(map_id);
        if (it == maps_.end()) {
            return {};
        }
        return it->second.dogs;
    }
    
    std::vector<std::shared_ptr<Dog>> GetAllDogs() const {
        std::vector<std::shared_ptr<Dog>> all_dogs;
        for (const auto& map_pair : maps_) {
            for (const auto& dog : map_pair.second.dogs) {
                all_dogs.push_back(dog);
            }
        }
        return all_dogs;
    }
    
    // Методы для работы с предметами
    void AddLootItem(const std::string& map_id, const LootItem& item) {
        auto it = maps_.find(map_id);
        if (it == maps_.end()) {
            throw std::runtime_error("Map not found: " + map_id);
        }
        it->second.loot_items.push_back(item);
    }
    
    std::vector<LootItem> GetLootItems(const std::string& map_id) const {
        auto it = maps_.find(map_id);
        if (it == maps_.end()) {
            return {};
        }
        return it->second.loot_items;
    }
    
    std::vector<LootItem> GetAllLootItems() const {
        std::vector<LootItem> all_items;
        for (const auto& map_pair : maps_) {
            for (const auto& item : map_pair.second.loot_items) {
                if (!item.is_collected) {
                    all_items.push_back(item);
                }
            }
        }
        return all_items;
    }
    
    // Методы для работы с игроками
    void AddPlayer(const std::string& token, const std::string& user_id, 
                   const std::string& map_id, std::shared_ptr<Dog> dog) {
        PlayerInfo player;
        player.token = token;
        player.user_id = user_id;
        player.dog = dog;
        player.map_id = map_id;
        players_[token] = player;
        
        AddDog(map_id, dog);
    }
    
    std::shared_ptr<Dog> GetPlayerDog(const std::string& token) const {
        auto it = players_.find(token);
        if (it == players_.end()) {
            return nullptr;
        }
        return it->second.dog;
    }
    
    bool HasPlayer(const std::string& token) const {
        return players_.find(token) != players_.end();
    }
    
    void RemovePlayer(const std::string& token) {
        players_.erase(token);
    }
    
    const std::unordered_map<std::string, PlayerInfo>& GetPlayers() const {
        return players_;
    }
    
    // Методы для работы с картами
    void AddMap(const std::string& map_id) {
        if (maps_.find(map_id) != maps_.end()) {
            throw std::runtime_error("Map already exists: " + map_id);
        }
        MapState new_map;
        new_map.map_id = map_id;
        maps_[map_id] = new_map;
    }
    
    bool HasMap(const std::string& map_id) const {
        return maps_.find(map_id) != maps_.end();
    }
    
    const std::unordered_map<std::string, MapState>& GetMaps() const {
        return maps_;
    }
    
    uint64_t GetGameTime() const {
        return game_time_ms_;
    }
    
    // Методы для сохранения и восстановления состояния
    void SaveState(serialization::GameState& state) const {
        state.game_time_ms = game_time_ms_;
        
        // Сохраняем список карт
        for (const auto& map_pair : maps_) {
            state.map_ids.push_back(map_pair.first);
        }
        
        // Сохраняем собак
        for (const auto& map_pair : maps_) {
            for (const auto& dog : map_pair.second.dogs) {
                state.dogs.emplace_back(*dog);
                state.dog_to_map[*dog->GetId()] = map_pair.first;
            }
        }
        
        // Сохраняем предметы
        for (const auto& map_pair : maps_) {
            for (const auto& item : map_pair.second.loot_items) {
                if (!item.is_collected) {
                    serialization::LootItemRepr item_repr;
                    item_repr.id = item.id;
                    item_repr.type = item.type;
                    item_repr.position = item.position;
                    state.loot_items.push_back(item_repr);
                }
            }
        }
        
        // Сохраняем игроков
        for (const auto& player_pair : players_) {
            const auto& player = player_pair.second;
            serialization::PlayerRepr player_repr;
            player_repr.token = player.token;
            player_repr.user_id = player.user_id;
            if (player.dog) {
                player_repr.dog_id = *player.dog->GetId();
                player_repr.map_id = player.map_id;
            }
            state.players.push_back(player_repr);
        }
    }
    
    void RestoreState(const serialization::GameState& state) {
        maps_.clear();
        players_.clear();
        
        game_time_ms_ = state.game_time_ms;
        
        // Восстанавливаем карты
        for (const auto& map_id : state.map_ids) {
            AddMap(map_id);
        }
        
        // Создаем маппинг ID собаки -> объект
        std::unordered_map<uint32_t, std::shared_ptr<Dog>> dog_map;
        
        // Восстанавливаем собак
        for (const auto& dog_repr : state.dogs) {
            auto dog = std::make_shared<model::Dog>(dog_repr.Restore());
            dog_map[*dog->GetId()] = dog;
            
            // Добавляем собаку на соответствующую карту
            auto it = state.dog_to_map.find(*dog->GetId());
            if (it != state.dog_to_map.end()) {
                auto map_it = maps_.find(it->second);
                if (map_it != maps_.end()) {
                    map_it->second.dogs.push_back(dog);
                }
            }
        }
        
        // Восстанавливаем предметы
        // Распределяем предметы по картам (равномерно или по какому-то правилу)
        // В данном случае просто добавляем все предметы на первую карту,
        // если карт несколько - нужно более сложное распределение
        if (!state.loot_items.empty() && !maps_.empty()) {
            auto it = maps_.begin();
            size_t map_index = 0;
            size_t total_maps = maps_.size();
            
            for (const auto& item_repr : state.loot_items) {
                LootItem item;
                item.id = item_repr.id;
                item.type = item_repr.type;
                item.position = item_repr.position;
                item.is_collected = false;
                
                // Распределяем предметы по картам по кругу
                auto map_it = maps_.begin();
                std::advance(map_it, map_index % total_maps);
                map_it->second.loot_items.push_back(item);
                map_index++;
            }
        }
        
        // Восстанавливаем игроков
        for (const auto& player_repr : state.players) {
            PlayerInfo player;
            player.token = player_repr.token;
            player.user_id = player_repr.user_id;
            player.map_id = player_repr.map_id;
            
            auto it = dog_map.find(player_repr.dog_id);
            if (it != dog_map.end()) {
                player.dog = it->second;
            }
            
            players_[player.token] = player;
        }
    }
    
    void UpdateMap(MapState& map_state, app::milliseconds delta) {
        for (auto& dog : map_state.dogs) {
            if (dog) {
                double speed_x = dog->GetSpeed().x;
                double speed_y = dog->GetSpeed().y;
                
                if (speed_x != 0 || speed_y != 0) {
                    geom::Point2D pos = dog->GetPosition();
                    double delta_seconds = delta.count() / 1000.0;
                    
                    pos.x += speed_x * delta_seconds;
                    pos.y += speed_y * delta_seconds;
                    
                    dog->SetPosition(pos);
                }
            }
        }
    }

private:
    std::unordered_map<std::string, MapState> maps_;
    std::unordered_map<std::string, PlayerInfo> players_;
    std::vector<ListenerPtr> listeners_;
    uint64_t game_time_ms_ = 0;
};

}  // namespace model