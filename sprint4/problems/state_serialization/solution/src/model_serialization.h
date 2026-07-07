#pragma once

#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/map.hpp>
#include <memory>
#include <string>
#include <vector>

#include "model.h"
#include "geom.h"

namespace geom {

template <typename Archive>
void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
    ar & point.x;
    ar & point.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version) {
    ar & vec.x;
    ar & vec.y;
}

}  // namespace geom

namespace model {

template <typename Archive>
void serialize(Archive& ar, FoundObject& obj, [[maybe_unused]] const unsigned version) {
    ar & (*obj.id);
    ar & (obj.type);
}

}  // namespace model

namespace serialization {

// DogRepr (DogRepresentation) - сериализованное представление класса Dog
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
                throw std::runtime_error("Failed to put bag content during restoration");
            }
        }
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & *id_;
        ar & name_;
        ar & pos_;
        ar & bag_capacity_;
        ar & speed_;
        ar & direction_;
        ar & score_;
        ar & bag_content_;
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

// LostObjectRepr - сериализованное представление потерянного на карте предмета
class LostObjectRepr {
public:
    LostObjectRepr() = default;

    LostObjectRepr(uint32_t id, unsigned type, geom::Point2D pos)
        : id_(id)
        , type_(type)
        , pos_(pos) {}

    [[nodiscard]] uint32_t GetId() const { return id_; }
    [[nodiscard]] unsigned GetType() const { return type_; }
    [[nodiscard]] geom::Point2D GetPosition() const { return pos_; }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & id_;
        ar & type_;
        ar & pos_;
    }

private:
    uint32_t id_ = 0;
    unsigned type_ = 0;
    geom::Point2D pos_;
};

// PlayerRepr - сохраняет информацию об авторизованном пользователе и его токене
class PlayerRepr {
public:
    PlayerRepr() = default;

    PlayerRepr(std::string token, uint32_t dog_id, std::string map_id)
        : token_(std::move(token))
        , dog_id_(dog_id)
        , map_id_(std::move(map_id)) {}

    [[nodiscard]] const std::string& GetToken() const { return token_; }
    [[nodiscard]] uint32_t GetDogId() const { return dog_id_; }
    [[nodiscard]] const std::string& GetMapId() const { return map_id_; }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & token_;
        ar & dog_id_;
        ar & map_id_;
    }

private:
    std::string token_;
    uint32_t dog_id_ = 0;
    std::string map_id_;
};

// SessionStateRepr - агрегирует состояние одной игровой сессии (конкретной карты)
class SessionStateRepr {
public:
    SessionStateRepr() = default;

    SessionStateRepr(std::string map_id, std::vector<DogRepr> dogs, std::vector<LostObjectRepr> lost_objects)
        : map_id_(std::move(map_id))
        , dogs_(std::move(dogs))
        , lost_objects_(std::move(lost_objects)) {}

    [[nodiscard]] const std::string& GetMapId() const { return map_id_; }
    [[nodiscard]] const std::vector<DogRepr>& GetDogs() const { return dogs_; }
    [[nodiscard]] const std::vector<LostObjectRepr>& GetLostObjects() const { return lost_objects_; }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & map_id_;
        ar & dogs_;
        ar & lost_objects_;
    }

private:
    std::string map_id_;
    std::vector<DogRepr> dogs_;
    std::vector<LostObjectRepr> lost_objects_;
};

// GameStateRepr - корневой объект сериализации всего сервера
class GameStateRepr {
public:
    GameStateRepr() = default;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & sessions_;
        ar & players_;
    }

    std::vector<SessionStateRepr> sessions_;
    std::vector<PlayerRepr> players_;
};

}  // namespace serialization