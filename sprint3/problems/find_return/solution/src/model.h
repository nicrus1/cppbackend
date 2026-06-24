#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <set>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct Position {
    double x = 0.0;
    double y = 0.0;
};

struct Speed {
    double vx = 0.0;
    double vy = 0.0;
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

inline std::string DirectionToString(Direction dir) {
    switch (dir) {
        case Direction::NORTH: return "U";
        case Direction::SOUTH: return "D";
        case Direction::WEST:  return "L";
        case Direction::EAST:  return "R";
    }
    return "U";
}

inline Direction StringToDirection(const std::string& str) {
    if (str == "U") return Direction::NORTH;
    if (str == "D") return Direction::SOUTH;
    if (str == "L") return Direction::WEST;
    if (str == "R") return Direction::EAST;
    return Direction::NORTH;
}

class Dog {
public:
    using Id = uint64_t;
    
    struct BagItem {
        uint64_t id;
        int type;
    };

    Dog(Id id, Point pos, double speed)
        : id_(id)
        , pos_{static_cast<double>(pos.x), static_cast<double>(pos.y)}
        , default_speed_(speed) {
    }

    Dog(Id id, Position pos, double speed)
        : id_(id)
        , pos_(pos)
        , default_speed_(speed) {
    }

    Id GetId() const {
        return id_;
    }

    const Position& GetPosition() const {
        return pos_;
    }

    void SetPosition(Position pos) {
        pos_ = pos;
    }
    
    void SetPosition(double x, double y) {
        pos_ = {x, y};
    }

    const Speed& GetSpeed() const {
        return speed_;
    }

    void SetSpeed(Speed speed) {
        speed_ = speed;
    }

    Direction GetDirection() const {
        return direction_;
    }

    void SetDirection(Direction dir) {
        direction_ = dir;
    }

    double GetDefaultSpeed() const {
        return default_speed_;
    }
    
    // Bag management
    void AddBagItem(uint64_t id, int type) {
        if (bag_.size() < bag_capacity_) {
            bag_.push_back({id, type});
        }
    }
    
    void ClearBag() {
        bag_.clear();
    }
    
    const std::vector<BagItem>& GetBag() const {
        return bag_;
    }
    
    size_t GetBagSize() const {
        return bag_.size();
    }
    
    bool IsBagFull() const {
        return bag_.size() >= bag_capacity_;
    }
    
    void SetBagCapacity(size_t capacity) {
        bag_capacity_ = capacity;
    }
    
    size_t GetBagCapacity() const {
        return bag_capacity_;
    }

private:
    Id id_;
    Position pos_;
    Speed speed_{0.0, 0.0};
    Direction direction_ = Direction::NORTH;
    double default_speed_;
    std::vector<BagItem> bag_;
    size_t bag_capacity_ = 3;
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);
    
    double GetDogSpeed() const noexcept {
        return dog_speed_.has_value() ? *dog_speed_ : default_dog_speed_;
    }
    
    void SetDogSpeed(double speed) {
        dog_speed_ = speed;
    }
    
    void SetDefaultDogSpeed(double speed) {
        default_dog_speed_ = speed;
    }
    
    void SetLootTypesCount(size_t count) noexcept {
        loot_types_count_ = count;
    }
    
    size_t GetLootTypesCount() const noexcept {
        return loot_types_count_;
    }
    
    void SetBagCapacity(size_t capacity) {
        bag_capacity_ = capacity;
    }
    
    size_t GetBagCapacity() const {
        return bag_capacity_.has_value() ? *bag_capacity_ : default_bag_capacity_;
    }
    
    void SetDefaultBagCapacity(size_t capacity) {
        default_bag_capacity_ = capacity;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    
    double default_dog_speed_ = 1.0;
    std::optional<double> dog_speed_;
    size_t loot_types_count_ = 0;
    size_t default_bag_capacity_ = 3;
    std::optional<size_t> bag_capacity_;
};

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
};

}  // namespace model