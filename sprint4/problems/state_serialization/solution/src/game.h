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
#include <random>

#include "model.h"
#include "app_listener.h"
#include "model_serialization.h"

namespace model {

struct RoadSegment {
    double x0 = 0, y0 = 0;
    double x1 = 0, y1 = 0;
    bool has_x1 = false;
    bool has_y1 = false;
};

struct Building {
    double x = 0, y = 0;
    double w = 0, h = 0;
};

struct Office {
    std::string id;
    double x = 0, y = 0;
    double offsetX = 0, offsetY = 0;
};

struct LootType {
    std::string name;
    std::string file;
    std::string type;
    double rotation = 0;
    std::string color;
    double scale = 0.01;
    uint32_t value = 0;
};

struct LootItem {
    uint32_t id = 0;
    uint32_t type = 0;
    geom::Point2D position;
    bool is_collected = false;
};

struct PlayerInfo {
    std::string token;
    std::string user_id;
    std::shared_ptr<Dog> dog;
    std::string map_id;
};

struct MapState {
    std::string map_id;
    std::string name;
    double default_dog_speed = 3.0;
    double dog_speed = 3.0;
    std::vector<std::shared_ptr<Dog>> dogs;
    std::vector<LootItem> loot_items;
    std::vector<RoadSegment> roads;
    std::vector<Building> buildings;
    std::vector<Office> offices;
    std::vector<LootType> loot_types;
    uint64_t last_loot_generation_time = 0;
    uint64_t loot_period_ms = 5000;
    double loot_probability = 0.5;
    uint64_t next_loot_id = 1;
    
    double map_width = 40.0;
    double map_height = 30.0;
};

class Token {
public:
    Token() = default;
    explicit Token(const std::string& token) : token_(token) {}
    
    const std::string& operator*() const { return token_; }
    operator std::string() const { return token_; }
    
    bool operator==(const Token& other) const { return token_ == other.token_; }
    bool operator!=(const Token& other) const { return token_ != other.token_; }
    
private:
    std::string token_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    
    Map(Id id, std::string name) : id_(std::move(id)), name_(std::move(name)) {}
    
    const Id& GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    
private:
    Id id_;
    std::string name_;
};

class Game {
public:
    using ListenerPtr = std::shared_ptr<app::ApplicationListener>;
    
    Game() : rng_(std::random_device{}()) {}
    
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
    
    void AddMap(const std::string& map_id, double dog_speed = 3.0) {
        if (maps_.find(map_id) != maps_.end()) {
            throw std::runtime_error("Map already exists: " + map_id);
        }
        MapState new_map;
        new_map.map_id = map_id;
        new_map.dog_speed = dog_speed;
        new_map.default_dog_speed = dog_speed;
        maps_[map_id] = new_map;
    }
    
    bool HasMap(const std::string& map_id) const {
        return maps_.find(map_id) != maps_.end();
    }
    
    const std::unordered_map<std::string, MapState>& GetMaps() const {
        return maps_;
    }
    
    std::unordered_map<std::string, MapState>& GetMaps() {
        return maps_;
    }
    
    MapState* GetMapState(const std::string& map_id) {
        auto it = maps_.find(map_id);
        if (it == maps_.end()) {
            return nullptr;
        }
        return &it->second;
    }
    
    const MapState* GetMapState(const std::string& map_id) const {
        auto it = maps_.find(map_id);
        if (it == maps_.end()) {
            return nullptr;
        }
        return &it->second;
    }
    
    const MapState* FindMap(const std::string& map_id) const {
        return GetMapState(map_id);
    }
    
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
    
    std::unordered_map<std::string, PlayerInfo>& GetPlayers() {
        return players_;
    }
    
    uint64_t GetGameTime() const {
        return game_time_ms_;
    }
    
    void SaveState(serialization::GameState& state) const {
        state.game_time_ms = game_time_ms_;
        
        for (const auto& map_pair : maps_) {
            state.map_ids.push_back(map_pair.first);
        }
        
        for (const auto& map_pair : maps_) {
            for (const auto& dog : map_pair.second.dogs) {
                if (dog) {
                    state.dogs.emplace_back(*dog);
                    state.dog_to_map[*dog->GetId()] = map_pair.first;
                }
            }
        }
        
        for (const auto& map_pair : maps_) {
            for (const auto& item : map_pair.second.loot_items) {
                if (!item.is_collected) {
                    serialization::LootItemRepr item_repr;
                    item_repr.id = item.id;
                    item_repr.type = item.type;
                    item_repr.position = item.position;
                    item_repr.map_id = map_pair.first;
                    state.loot_items.push_back(item_repr);
                }
            }
        }
        
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
        auto existing_maps = maps_;
        maps_.clear();
        players_.clear();
        
        game_time_ms_ = state.game_time_ms;
        
        for (const auto& map_id : state.map_ids) {
            if (existing_maps.find(map_id) != existing_maps.end()) {
                maps_[map_id] = existing_maps[map_id];
            } else {
                AddMap(map_id);
            }
        }
        
        std::unordered_map<uint32_t, std::shared_ptr<Dog>> dog_map;
        
        for (const auto& dog_repr : state.dogs) {
            auto dog = std::make_shared<model::Dog>(dog_repr.Restore());
            uint32_t dog_id = *dog->GetId();
            dog_map[dog_id] = dog;
            
            auto it = state.dog_to_map.find(dog_id);
            if (it != state.dog_to_map.end()) {
                auto map_it = maps_.find(it->second);
                if (map_it != maps_.end()) {
                    map_it->second.dogs.push_back(dog);
                }
            }
        }
        
        for (const auto& item_repr : state.loot_items) {
            LootItem item;
            item.id = item_repr.id;
            item.type = item_repr.type;
            item.position = item_repr.position;
            item.is_collected = false;
            
            auto map_it = maps_.find(item_repr.map_id);
            if (map_it != maps_.end()) {
                map_it->second.loot_items.push_back(item);
                if (item.id >= map_it->second.next_loot_id) {
                    map_it->second.next_loot_id = item.id + 1;
                }
            }
        }
        
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
                    
                    pos.x += speed_x * delta_seconds * map_state.dog_speed;
                    pos.y += speed_y * delta_seconds * map_state.dog_speed;
                    
                    pos.x = std::max(0.0, std::min(map_state.map_width, pos.x));
                    pos.y = std::max(0.0, std::min(map_state.map_height, pos.y));
                    
                    dog->SetPosition(pos);
                }
            }
        }
        
        if (map_state.loot_period_ms > 0 && !map_state.loot_types.empty()) {
            map_state.last_loot_generation_time += delta.count();
            
            while (map_state.last_loot_generation_time >= map_state.loot_period_ms) {
                map_state.last_loot_generation_time -= map_state.loot_period_ms;
                
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                if (dist(rng_) < map_state.loot_probability) {
                    LootItem item;
                    item.id = map_state.next_loot_id++;
                    
                    std::uniform_int_distribution<size_t> type_dist(0, map_state.loot_types.size() - 1);
                    item.type = type_dist(rng_);
                    
                    std::uniform_real_distribution<double> x_dist(1.0, map_state.map_width - 1.0);
                    std::uniform_real_distribution<double> y_dist(1.0, map_state.map_height - 1.0);
                    item.position.x = x_dist(rng_);
                    item.position.y = y_dist(rng_);
                    item.is_collected = false;
                    
                    map_state.loot_items.push_back(item);
                }
            }
        }
    }

private:
    std::unordered_map<std::string, MapState> maps_;
    std::unordered_map<std::string, PlayerInfo> players_;
    std::vector<ListenerPtr> listeners_;
    uint64_t game_time_ms_ = 0;
    std::mt19937 rng_;
};

}  // namespace model