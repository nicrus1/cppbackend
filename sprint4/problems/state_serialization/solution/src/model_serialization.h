#pragma once

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/optional.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>

#include "model.h"
#include "player.h"
#include "players.h"
#include "player_tokens.h"
#include "dog.h"
#include "loot_manager.h"

namespace serialization {

// Структура для сериализации Dog
class DogRepr {
public:
    DogRepr() = default;
    
    explicit DogRepr(const model::Dog& dog)
        : id_(*dog.GetId())
        , pos_(dog.GetPosition())
        , speed_(dog.GetSpeed())
        , direction_(dog.GetDirection())
        , score_(dog.GetScore())
        , retirement_time_(dog.GetRetirementTime().count())
        , total_play_time_(dog.GetTotalPlayTime().count()) {
    }
    
    model::Dog Restore() const {
        model::Dog dog{model::Dog::Id{id_}, pos_, 1.0}; // скорость будет установлена из карты
        dog.SetSpeed(speed_);
        dog.SetDirection(direction_);
        dog.AddScore(score_);
        dog.SetRetirementTime(std::chrono::milliseconds(retirement_time_));
        dog.SetTotalPlayTime(std::chrono::milliseconds(total_play_time_));
        return dog;
    }
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& pos_;
        ar& speed_;
        ar& direction_;
        ar& score_;
        ar& retirement_time_;
        ar& total_play_time_;
    }
    
    uint32_t GetId() const { return id_; }
    
private:
    uint32_t id_ = 0;
    model::Position pos_;
    model::Speed speed_;
    model::Direction direction_ = model::Direction::NORTH;
    int score_ = 0;
    int64_t retirement_time_ = 60000;
    int64_t total_play_time_ = 0;
};

// Структура для сериализации LootItem
struct LootItemRepr {
    uint64_t id = 0;
    int type = 0;
    model::Position pos;
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id;
        ar& type;
        ar& pos;
    }
};

// Структура для сериализации Player
struct PlayerRepr {
    std::string token;
    std::string name;
    std::string map_id;
    uint32_t dog_id = 0;
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& token;
        ar& name;
        ar& map_id;
        ar& dog_id;
    }
};

// Полное состояние игры
struct GameState {
    uint64_t game_time_ms = 0;
    
    // Собаки с привязкой к картам
    std::vector<DogRepr> dogs;
    std::unordered_map<uint32_t, std::string> dog_to_map;
    
    // Предметы с привязкой к картам
    std::vector<LootItemRepr> loot_items;
    std::unordered_map<uint64_t, std::string> loot_to_map;
    
    // Игроки
    std::vector<PlayerRepr> players;
    
    // Время бездействия собак
    std::unordered_map<uint32_t, int64_t> idle_time; // player_id -> ms
    
    // Генерация трофеев
    struct LootManagerState {
        std::string map_id;
        uint64_t next_id = 0;
        int64_t time_without_loot = 0;
        
        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& map_id;
            ar& next_id;
            ar& time_without_loot;
        }
    };
    std::vector<LootManagerState> loot_managers;
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& game_time_ms;
        ar& dogs;
        ar& dog_to_map;
        ar& loot_items;
        ar& loot_to_map;
        ar& players;
        ar& idle_time;
        ar& loot_managers;
    }
};

class StateSerializer {
public:
    static bool SaveToFile(const GameState& state, const std::string& filepath) {
        try {
            std::string temp_path = filepath + ".tmp";
            std::ofstream ofs(temp_path);
            if (!ofs) {
                return false;
            }
            
            boost::archive::text_oarchive oa(ofs);
            oa << state;
            ofs.close();
            
            std::error_code ec;
            std::filesystem::rename(temp_path, filepath, ec);
            return !ec;
        } catch (...) {
            return false;
        }
    }
    
    static bool LoadFromFile(GameState& state, const std::string& filepath) {
        try {
            if (!std::filesystem::exists(filepath)) {
                return false;
            }
            
            std::ifstream ifs(filepath);
            if (!ifs) {
                return false;
            }
            
            boost::archive::text_iarchive ia(ifs);
            ia >> state;
            return true;
        } catch (...) {
            return false;
        }
    }
};

} // namespace serialization