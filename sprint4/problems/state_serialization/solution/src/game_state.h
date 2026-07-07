#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <random>
#include <chrono>
#include <algorithm>

#include "model.h"
#include "game.h"
#include "db/record_manager.h"

namespace game {

struct PlayerState {
    std::string player_id;
    geom::Point2D pos;
    geom::Vec2D speed;
    model::Direction dir;
    int score = 0;
};

struct LootState {
    uint32_t type;
    geom::Point2D pos;
};

class GameState {
public:
    explicit GameState(model::Game& game) 
        : game_(game)
        , rng_(std::random_device{}())
        , retirement_time_(std::chrono::seconds(0)) {
    }
    
    struct JoinResult {
        model::Token token;
        uint32_t player_id;
    };
    
    JoinResult JoinGame(const std::string& user_name, const model::Map::Id& map_id) {
        // Проверяем существование карты
        auto map_state = game_.GetMapState(*map_id);
        if (!map_state) {
            throw std::runtime_error("Map not found");
        }
        
        // Генерируем токен
        std::string token_str = GenerateToken();
        model::Token token{token_str};
        
        // Создаем собаку
        uint32_t dog_id_num = game_.GetAllDogs().size() + 1;
        auto dog_id = model::Dog::Id{dog_id_num};
        
        // Выбираем стартовую позицию
        geom::Point2D pos;
        if (!map_state->offices.empty()) {
            const auto& office = map_state->offices[0];
            pos.x = office.x + office.offsetX;
            pos.y = office.y + office.offsetY;
        } else {
            pos.x = 5.0;
            pos.y = 5.0;
        }
        
        auto dog = std::make_shared<model::Dog>(dog_id, user_name, pos, 3);
        
        // Добавляем игрока
        game_.AddPlayer(token_str, user_name, *map_id, dog);
        
        // Сохраняем время присоединения
        join_times_[token_str] = std::chrono::steady_clock::now();
        
        return {token, dog_id_num};
    }
    
    bool ValidateToken(const model::Token& token) const {
        return game_.HasPlayer(*token);
    }
    
    std::vector<PlayerState> GetGameState(const model::Token& token) const {
        std::vector<PlayerState> result;
        
        auto player_dog = game_.GetPlayerDog(*token);
        if (!player_dog) {
            return result;
        }
        
        // Получаем всех игроков на той же карте
        auto map_state = game_.GetMapState(game_.GetPlayers().at(*token).map_id);
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
    
    std::unordered_map<uint32_t, std::pair<uint32_t, geom::Point2D>> GetLootState(const model::Token& token) const {
        std::unordered_map<uint32_t, std::pair<uint32_t, geom::Point2D>> result;
        
        auto player_dog = game_.GetPlayerDog(*token);
        if (!player_dog) {
            return result;
        }
        
        auto map_state = game_.GetMapState(game_.GetPlayers().at(*token).map_id);
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
    
    std::unordered_map<std::string, std::string> GetPlayersOnMap(const model::Token& token) const {
        std::unordered_map<std::string, std::string> result;
        
        auto it = game_.GetPlayers().find(*token);
        if (it == game_.GetPlayers().end()) {
            return result;
        }
        
        const auto& player = it->second;
        auto map_state = game_.GetMapState(player.map_id);
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
    
    void SetDogDirection(const model::Token& token, model::Direction dir) {
        auto dog = game_.GetPlayerDog(*token);
        if (!dog) {
            return;
        }
        
        dog->SetDirection(dir);
        
        // Устанавливаем скорость в зависимости от направления
        geom::Vec2D speed{0, 0};
        switch (dir) {
            case model::Direction::NORTH:
                speed.y = -1.0;
                break;
            case model::Direction::SOUTH:
                speed.y = 1.0;
                break;
            case model::Direction::WEST:
                speed.x = -1.0;
                break;
            case model::Direction::EAST:
                speed.x = 1.0;
                break;
        }
        dog->SetSpeed(speed);
    }
    
    void StopDog(const model::Token& token) {
        auto dog = game_.GetPlayerDog(*token);
        if (!dog) {
            return;
        }
        dog->SetSpeed({0, 0});
    }
    
    void ProcessTick(int64_t delta_ms) {
        // Обновляем состояние игры через Game::Tick
        game_.Tick(app::milliseconds(delta_ms));
        
        // Проверяем сбор предметов
        CollectLoot();
        
        // Проверяем выход игроков по времени
        CheckRetirement();
    }
    
    void SetLootGeneratorConfig(double period, double probability) {
        loot_period_ = period;
        loot_probability_ = probability;
        
        // Обновляем настройки для всех карт
        for (auto& map_pair : game_.GetMaps()) {
            auto& map_state = const_cast<model::MapState&>(map_pair.second);
            map_state.loot_period_ms = static_cast<uint64_t>(period * 1000);
            map_state.loot_probability = probability;
        }
    }
    
    void SetDogRetirementTime(double seconds) {
        retirement_time_ = std::chrono::milliseconds(static_cast<int64_t>(seconds * 1000));
    }
    
    void SetRecordManager(std::shared_ptr<db::RecordManager> manager) {
        record_manager_ = manager;
    }
    
private:
    void CollectLoot() {
        for (auto& map_pair : game_.GetMaps()) {
            auto& map_state = const_cast<model::MapState&>(map_pair.second);
            
            for (auto& dog : map_state.dogs) {
                if (!dog) continue;
                
                auto dog_pos = dog->GetPosition();
                
                for (auto& item : map_state.loot_items) {
                    if (item.is_collected) continue;
                    
                    // Проверяем расстояние до предмета
                    double dx = dog_pos.x - item.position.x;
                    double dy = dog_pos.y - item.position.y;
                    double dist = std::sqrt(dx * dx + dy * dy);
                    
                    if (dist < 1.0) { // Радиус сбора
                        item.is_collected = true;
                        
                        // Добавляем очки
                        if (!map_state.loot_types.empty() && item.type < map_state.loot_types.size()) {
                            dog->AddScore(map_state.loot_types[item.type].value);
                        }
                        
                        // Сохраняем рекорд
                        if (record_manager_) {
                            record_manager_->AddRecord(
                                dog->GetName(),
                                dog->GetScore(),
                                static_cast<int>(game_.GetGameTime() / 1000)
                            );
                        }
                    }
                }
            }
        }
    }
    
    void CheckRetirement() {
        if (retirement_time_.count() == 0) {
            return;
        }
        
        auto now = std::chrono::steady_clock::now();
        std::vector<std::string> to_remove;
        
        for (const auto& [token, join_time] : join_times_) {
            auto elapsed = now - join_time;
            if (elapsed >= retirement_time_) {
                to_remove.push_back(token);
            }
        }
        
        for (const auto& token : to_remove) {
            game_.RemovePlayer(token);
            join_times_.erase(token);
        }
    }
    
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
    std::chrono::milliseconds retirement_time_;
    double loot_period_ = 5.0;
    double loot_probability_ = 0.5;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> join_times_;
    std::shared_ptr<db::RecordManager> record_manager_;
};

// Вспомогательные функции для конвертации Direction
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