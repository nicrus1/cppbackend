#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

namespace model {

using Dimension = int;
using Coord = double;

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

struct Speed {
    double vx = 0.0;
    double vy = 0.0;
};

struct Position {
    double x = 0.0;
    double y = 0.0;
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

class Road {
    struct HorizontalTag {
    };
    struct VerticalTag {
    };

public:
    static constexpr HorizontalTag HORIZONTAL{};
    static constexpr VerticalTag VERTICAL{};

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
    using Id = std::string;

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
    using Id = std::string;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name)
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road) {
        roads_.push_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.push_back(building);
    }

    void AddOffice(Office office) {
        offices_.push_back(std::move(office));
    }

private:
    Id id_;
    std::string name_;

    Roads roads_;
    Buildings buildings_;
    Offices offices_;
};

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map) {
        const size_t index = maps_.size();

        if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index);
            inserted) {
            maps_.emplace_back(std::move(map));
        } else {
            throw std::invalid_argument("Map with id " + it->first + " already exists");
        }
    }

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id);
            it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

private:
    using MapIdHasher = std::hash<Map::Id>;

    std::unordered_map<Map::Id, size_t, MapIdHasher> map_id_to_index_;
    std::vector<Map> maps_;
};

} // namespace model