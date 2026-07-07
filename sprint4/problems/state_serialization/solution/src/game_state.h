#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <random>
#include <chrono>
#include <algorithm>
#include <iostream>

#include "model.h"
#include "game.h"

namespace game {

struct PlayerState {
    std::string player_id;
    geom::Point2D pos;
    geom::Vec2D speed;
    model::Direction dir;
    int score = 0;
};

class GameState {
public:
    explicit GameState(model::Game& game) 
        : game_(game)
        , rng_(std::random_device{}()) {
    }
    
    struct JoinResult {
        std::string token;
        uint32_t player_id;
    };
    
    JoinResult JoinGame(const std::string& user_name, const std::string& map_id) {
        std::cout << "JoinGame called: user=" << user_name << ", map_id=" << map_id << std::endl;
        
        auto map_state = game_.GetMapState(map_id);
        if (!map_state) {
            std::cerr << "Map not found: " << map_id << std::endl;
            std::cerr << "Available maps:" << std::endl;
            for (const auto& pair : game_.GetMaps()) {
                std::cerr << "  " << pair.first << std::endl;
            }
            throw std::runtime_error("Map not found: " + map_id);
        }
        
        std::string token_str = GenerateToken();
        
        uint32_t dog_id_num = game_.GetAllDogs().size() + 1;
        auto dog_id = model::Dog::Id{dog_id_num};
        
        geom::Point2D pos(5.0, 5.0);
        if (!map_state->offices.empty()) {
            const auto& office = map_state->offices[0];
            pos.x = office.x + office.offsetX;
            pos.y = office.y + office.offsetY;
        }
        
        auto dog = std::make_shared<model::Dog>(dog_id, user_name, pos, 3);
        game_.AddPlayer(token_str, user_name, map_id, dog);
        
        return {token_str, dog_id_num};
    }
    
    bool ValidateToken(const std::string& token) const {
        return game_.HasPlayer(token);
    }
    
    std::vector<PlayerState> GetGameState(const std::string& token) const {
        std::vector<PlayerState> result;
        
        auto player_dog = game_.GetPlayerDog(token);
        if (!player_dog) {
            return result;
        }
        
        auto it = game_.GetPlayers().find(token);
        if (it == game_.GetPlayers().end()) {
            return result;
        }
        
        auto map_state = game_.GetMapState(it->second.map_id);
        if (!map_state) {
            return result;
        }
        
        for (const auto& dog : map_state->dogs) {
            if (!dog) continue;
            PlayerState state;
            state.player_id = std::to_string(*dog->GetId());
            state.pos = dog->GetPosition();
            state.speed = dog->GetSpeed();
            state.dir = dog->GetDirection();
            state.score = dog->GetScore();
            result.push_back(state);
        }
        
        return result;
    }
    
    std::unordered_map<uint32_t, std::pair<uint32_t, geom::Point2D>> 
    GetLootState(const std::string& token) const {
        std::unordered_map<uint32_t, std::pair<uint32_t, geom::Point2D>> result;
        
        auto player_dog = game_.GetPlayerDog(token);
        if (!player_dog) {
            return result;
        }
        
        auto it = game_.GetPlayers().find(token);
        if (it == game_.GetPlayers().end()) {
            return result;
        }
        
        auto map_state = game_.GetMapState(it->second.map_id);
        if (!map_state) {
            return result;
        }
        
        for (const auto& item : map_state->loot_items) {
            if (!item.is_collected) {
                result[item.id] = {item.type, item.position};
            }
        }
        
        return result;
    }
    
    std::unordered_map<std::string, std::string> GetPlayersOnMap(const std::string& token) const {
        std::unordered_map<std::string, std::string> result;
        
        auto it = game_.GetPlayers().find(token);
        if (it == game_.GetPlayers().end()) {
            return result;
        }
        
        auto map_state = game_.GetMapState(it->second.map_id);
        if (!map_state) {
            return result;
        }
        
        for (const auto& dog : map_state->dogs) {
            if (dog) {
                result[std::to_string(*dog->GetId())] = dog->GetName();
            }
        }
        
        return result;
    }
    
    void SetDogDirection(const std::string& token, model::Direction dir) {
        auto dog = game_.GetPlayerDog(token);
        if (!dog) return;
        
        dog->SetDirection(dir);
        geom::Vec2D speed{0, 0};
        switch (dir) {
            case model::Direction::NORTH: speed.y = -1.0; break;
            case model::Direction::SOUTH: speed.y = 1.0; break;
            case model::Direction::WEST: speed.x = -1.0; break;
            case model::Direction::EAST: speed.x = 1.0; break;
        }
        dog->SetSpeed(speed);
    }
    
    void StopDog(const std::string& token) {
        auto dog = game_.GetPlayerDog(token);
        if (!dog) return;
        dog->SetSpeed({0, 0});
    }
    
    void ProcessTick(int64_t delta_ms) {
        game_.Tick(app::milliseconds(delta_ms));
    }
    
    void SetLootGeneratorConfig(double period, double probability) {
        for (auto& map_pair : game_.GetMaps()) {
            map_pair.second.loot_period_ms = static_cast<uint64_t>(period * 1000);
            map_pair.second.loot_probability = probability;
        }
    }
    
    void SetDogRetirementTime(double seconds) {
        // Упрощенная реализация
    }
    
    void SetRecordManager(std::shared_ptr<void> manager) {
        // Упрощенная реализация
    }
    
private:
    std::string GenerateToken() {
        static const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::uniform_int_distribution<> dist(0, chars.size() - 1);
        std::string token;
        token.reserve(32);
        for (int i = 0; i < 32; ++i) {
            token.push_back(chars[dist(rng_)]);
        }
        return token;
    }
    
    model::Game& game_;
    std::mt19937 rng_;
};

inline std::string DirectionToString(model::Direction dir) {
    switch (dir) {
        case model::Direction::NORTH: return "N";
        case model::Direction::SOUTH: return "S";
        case model::Direction::WEST: return "W";
        case model::Direction::EAST: return "E";
        default: return "N";
    }
}

inline model::Direction StringToDirection(const std::string& str) {
    if (str == "N") return model::Direction::NORTH;
    if (str == "S") return model::Direction::SOUTH;
    if (str == "W") return model::Direction::WEST;
    if (str == "E") return model::Direction::EAST;
    return model::Direction::NORTH;
}

} // namespace game