#pragma once

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/optional.hpp>
#include <fstream>
#include <iostream>

#include "model.h"
#include "game.h"

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
    ar& *obj.id;
    ar& obj.type;
}

template <typename Archive>
void serialize(Archive& ar, Dog::Id& id, [[maybe_unused]] const unsigned version) {
    ar& *id;
}

}  // namespace model

namespace serialization {

// DogRepr - сериализованное представление класса Dog
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog)
        : id_(*dog.GetId())
        , name_(dog.GetName())
        , pos_(dog.GetPosition())
        , bag_capacity_(dog.GetBagCapacity())
        , speed_(dog.GetSpeed())
        , direction_(dog.GetDirection())
        , score_(dog.GetScore())
        , bag_content_(dog.GetBagContent()) {
    }

    [[nodiscard]] model::Dog Restore() const {
        model::Dog dog{model::Dog::Id{id_}, name_, pos_, bag_capacity_};
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
        ar& id_;
        ar& name_;
        ar& pos_;
        ar& bag_capacity_;
        ar& speed_;
        ar& direction_;
        ar& score_;
        ar& bag_content_;
    }

private:
    uint32_t id_ = 0;
    std::string name_;
    geom::Point2D pos_;
    size_t bag_capacity_ = 0;
    geom::Vec2D speed_;
    model::Direction direction_ = model::Direction::NORTH;
    model::Score score_ = 0;
    model::Dog::BagContent bag_content_;
};

// LootItemRepr - сериализованное представление предмета
struct LootItemRepr {
    uint32_t id = 0;
    uint32_t type = 0;
    geom::Point2D position;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id;
        ar& type;
        ar& position;
    }
};

// PlayerRepr - сериализованное представление игрока
struct PlayerRepr {
    std::string token;
    std::string user_id;
    uint32_t dog_id = 0;
    std::string map_id;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& token;
        ar& user_id;
        ar& dog_id;
        ar& map_id;
    }
};

// GameState - полное состояние игры
struct GameState {
    uint64_t game_time_ms = 0;
    std::vector<std::string> map_ids;
    std::vector<DogRepr> dogs;
    std::vector<LootItemRepr> loot_items;
    std::vector<PlayerRepr> players;
    
    // Для восстановления соответствия собака->карта
    std::unordered_map<uint32_t, std::string> dog_to_map;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& game_time_ms;
        ar& map_ids;
        ar& dogs;
        ar& loot_items;
        ar& players;
        ar& dog_to_map;
    }
};

// StateSerializer - утилиты для сохранения/загрузки
class StateSerializer {
public:
    static bool SaveToFile(const GameState& state, const std::string& filepath) {
        try {
            // Сначала сохраняем во временный файл
            std::string temp_path = filepath + ".tmp";
            std::ofstream ofs(temp_path);
            if (!ofs) {
                std::cerr << "Failed to open temporary file: " << temp_path << std::endl;
                return false;
            }
            
            boost::archive::text_oarchive oa(ofs);
            oa << state;
            ofs.close();
            
            // Атомарно переименовываем
            std::error_code ec;
            std::filesystem::rename(temp_path, filepath, ec);
            if (ec) {
                std::cerr << "Failed to rename file: " << ec.message() << std::endl;
                return false;
            }
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Exception during save: " << e.what() << std::endl;
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
                std::cerr << "Failed to open file: " << filepath << std::endl;
                return false;
            }
            
            boost::archive::text_iarchive ia(ifs);
            ia >> state;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Exception during load: " << e.what() << std::endl;
            return false;
        }
    }
};

}  // namespace serialization