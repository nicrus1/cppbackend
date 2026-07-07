#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <chrono>
#include <algorithm>
#include <stdexcept>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

// Геометрические типы
namespace geom {
    struct Point2D {
        double x = 0.0;
        double y = 0.0;
        
        bool operator==(const Point2D& other) const {
            return x == other.x && y == other.y;
        }
        
        bool operator!=(const Point2D& other) const {
            return !(*this == other);
        }
    };
} // namespace geom

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
    
    bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }
    
    bool operator!=(const Position& other) const {
        return !(*this == other);
    }
};

struct Speed {
    double vx = 0.0;
    double vy = 0.0;
    
    bool operator==(const Speed& other) const {
        return vx == other.vx && vy == other.vy;
    }
    
    bool operator!=(const Speed& other) const {
        return !(*this == other);
    }
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

// --- Dog ---
class Dog {
public:
    using Id = uint64_t;

    Dog() = default;
    
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
    
    Dog(Id id, geom::Point2D pos, double speed)
        : id_(id)
        , pos_{pos.x, pos.y}
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
    
    void SetPosition(const geom::Point2D& pos) {
        pos_ = {pos.x, pos.y};
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

    void SetRetirementTime(std::chrono::milliseconds time) {
        retirement_time_ = time;
    }
    
    std::chrono::milliseconds GetRetirementTime() const {
        return retirement_time_;
    }
    
    void SetTotalPlayTime(std::chrono::milliseconds time) {
        total_play_time_ = time;
    }
    
    std::chrono::milliseconds GetTotalPlayTime() const {
        return total_play_time_;
    }
    
    int GetScore() const {
        return score_;
    }
    
    void AddScore(int value) {
        score_ += value;
    }

private:
    Id id_ = 0;
    Position pos_{0.0, 0.0};
    Speed speed_{0.0, 0.0};
    Direction direction_ = Direction::NORTH;
    double default_speed_ = 1.0;
    int score_ = 0;
    
    std::chrono::milliseconds retirement_time_{60000};
    std::chrono::milliseconds total_play_time_{0};
};

// --- Road ---
class Road {
public:
    using Id = uint64_t;
    static constexpr int HORIZONTAL = 0;
    static constexpr int VERTICAL = 1;

    Road() = default;
    
    Road(int type, Point start, Coord end)
        : type_(type)
        , start_(start)
        , end_((type == HORIZONTAL) ? Point{end, start.y} : Point{start.x, end}) {
    }

    bool IsHorizontal() const {
        return type_ == HORIZONTAL;
    }

    bool IsVertical() const {
        return type_ == VERTICAL;
    }

    const Point& GetStart() const {
        return start_;
    }

    const Point& GetEnd() const {
        return end_;
    }

private:
    int type_ = HORIZONTAL;
    Point start_{0, 0};
    Point end_{0, 0};
};

// --- Building ---
class Building {
public:
    Building() = default;
    
    Building(Rectangle rect)
        : bounds_(rect) {
    }

    const Rectangle& GetBounds() const {
        return bounds_;
    }

private:
    Rectangle bounds_{{0, 0}, {0, 0}};
};

// --- Office ---
class Office {
public:
    struct Id : public util::Tagged<std::string, Office> {
        using Tagged::Tagged;
    };

    Office() = default;
    
    Office(Id id, Point pos, Offset offset)
        : id_(std::move(id))
        , pos_(pos)
        , offset_(offset) {
    }

    const Id& GetId() const {
        return id_;
    }

    const Point& GetPosition() const {
        return pos_;
    }

    const Offset& GetOffset() const {
        return offset_;
    }

private:
    Id id_{""};
    Point pos_{0, 0};
    Offset offset_{0, 0};
};

// --- Map ---
class Map {
public:
    struct Id : public util::Tagged<std::string, Map> {
        using Tagged::Tagged;
    };

    Map() = default;
    
    Map(Id id, std::string name)
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const {
        return id_;
    }

    const std::string& GetName() const {
        return name_;
    }

    void AddRoad(Road road) {
        roads_.push_back(std::move(road));
    }

    const std::vector<Road>& GetRoads() const {
        return roads_;
    }

    void AddBuilding(Building building) {
        buildings_.push_back(std::move(building));
    }

    const std::vector<Building>& GetBuildings() const {
        return buildings_;
    }

    void AddOffice(Office office);

    const std::vector<Office>& GetOffices() const {
        return offices_;
    }

    void SetDogSpeed(double speed) {
        dog_speed_ = speed;
    }

    void SetDefaultDogSpeed(double speed) {
        default_dog_speed_ = speed;
    }

    double GetDogSpeed() const {
        return dog_speed_.value_or(default_dog_speed_);
    }

    void SetLootTypesCount(size_t count) {
        loot_types_count_ = count;
    }

    size_t GetLootTypesCount() const {
        return loot_types_count_;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_{""};
    std::string name_;
    std::vector<Road> roads_;
    std::vector<Building> buildings_;
    std::vector<Office> offices_;
    OfficeIdToIndex warehouse_id_to_index_;
    double default_dog_speed_ = 1.0;
    std::optional<double> dog_speed_;
    size_t loot_types_count_ = 0;
};

// --- Game ---
class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const {
        auto it = map_id_to_index_.find(id);
        if (it == map_id_to_index_.end()) {
            return nullptr;
        }
        return &maps_.at(it->second);
    }

private:
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, util::TaggedHasher<Map::Id>>;

    Maps maps_;
    MapIdToIndex map_id_to_index_;
};

}  // namespace model