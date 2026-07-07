#pragma once

#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>

#include "model.h"

namespace geom {

template <typename Archive>
void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
    ar& point.x;
    ar& point.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version) {
    ar& vec.x;
    ar& vec.y;
}

}  // namespace geom

namespace model {

template <typename Archive>
void serialize(Archive& ar, FoundObject& obj, [[maybe_unused]] const unsigned version) {
    ar& (*obj.id);
    ar& obj.type;
}

template <typename Archive>
void serialize(Archive& ar, Dog::Id& id, [[maybe_unused]] const unsigned version) {
    ar& (*id);
}

}  // namespace model

namespace serialization {

// DogRepr - сериализованное представление класса Dog
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog)
        : id_(dog.GetId())
        , name_(dog.GetName())
        , pos_(dog.GetPosition())
        , bag_capacity_(dog.GetBagCapacity())
        , speed_(dog.GetSpeed())
        , direction_(dog.GetDirection())
        , score_(dog.GetScore())
        , bag_content_(dog.GetBagContent()) {
    }

    [[nodiscard]] model::Dog Restore() const {
        model::Dog dog{id_, name_, pos_, bag_capacity_};
        dog.SetSpeed(speed_);
        dog.SetDirection(direction_);
        dog.AddScore(score_);
        for (const auto& item : bag_content_) {
            if (!dog.PutToBag(item)) {
                throw std::runtime_error("Failed to put bag content");
            }
        }
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar&* id_;
        ar& name_;
        ar& pos_;
        ar& bag_capacity_;
        ar& speed_;
        ar& direction_;
        ar& score_;
        ar& bag_content_;
    }

private:
    model::Dog::Id id_ = model::Dog::Id{0u};
    std::string name_;
    geom::Point2D pos_;
    size_t bag_capacity_ = 0;
    geom::Vec2D speed_;
    model::Direction direction_ = model::Direction::NORTH;
    model::Score score_ = 0;
    model::Dog::BagContent bag_content_;
};

// PlayerRepr - сериализованное представление игрока
struct PlayerRepr {
    std::string token;
    std::string dog_id;
    std::string user_id;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& token;
        ar& dog_id;
        ar& user_id;
    }
};

// LootItemRepr - сериализованное представление предмета
struct LootItemRepr {
    uint32_t id;
    uint32_t type;
    geom::Point2D position;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id;
        ar& type;
        ar& position;
    }
};

// GameState - полное состояние игры для сериализации
struct GameState {
    std::vector<DogRepr> dogs;
    std::vector<LootItemRepr> loot_items;
    std::vector<PlayerRepr> players;
    uint64_t game_time_ms = 0;
    
    // Карты и их состояние
    struct MapState {
        std::string map_id;
        std::vector<DogRepr> map_dogs;
        std::vector<LootItemRepr> map_loot;
        
        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& map_id;
            ar& map_dogs;
            ar& map_loot;
        }
    };
    std::vector<MapState> maps;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& dogs;
        ar& loot_items;
        ar& players;
        ar& game_time_ms;
        ar& maps;
    }
};

// Сохранение и загрузка состояния
class StateSerializer {
public:
    static bool SaveToFile(const GameState& state, const std::string& file_path) {
        try {
            // Создаем временный файл
            std::string temp_path = file_path + ".tmp";
            
            // Сохраняем во временный файл
            {
                std::ofstream ofs(temp_path);
                if (!ofs.is_open()) {
                    std::cerr << "Failed to open temp file: " << temp_path << std::endl;
                    return false;
                }
                boost::archive::text_oarchive oa(ofs);
                oa << state;
            }
            
            // Проверяем, что временный файл создан
            if (!std::filesystem::exists(temp_path)) {
                std::cerr << "Temp file was not created: " << temp_path << std::endl;
                return false;
            }
            
            // Атомарно переименовываем
            std::filesystem::rename(temp_path, file_path);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "SaveToFile exception: " << e.what() << std::endl;
            return false;
        }
    }

    static bool LoadFromFile(GameState& state, const std::string& file_path) {
        try {
            if (!std::filesystem::exists(file_path)) {
                std::cerr << "State file does not exist: " << file_path << std::endl;
                return false;
            }
            
            std::ifstream ifs(file_path);
            if (!ifs.is_open()) {
                std::cerr << "Failed to open state file: " << file_path << std::endl;
                return false;
            }
            
            boost::archive::text_iarchive ia(ifs);
            ia >> state;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "LoadFromFile exception: " << e.what() << std::endl;
            return false;
        }
    }
};

}  // namespace serialization