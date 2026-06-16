#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>

#include "geom.h"
#include "tagged.h"
#include "loot_generator.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    double x, y;
};

struct Road {
    Point start;
    Point end;
};

struct Building {
    Rectangle bounds;
};

struct Office {
    using Id = util::Tagged<std::string, Office>;
    Id id;
    Point position;
    Offset offset;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name, size_t loot_types_count)
        : id_(std::move(id)), name_(std::move(name)), loot_types_count_(loot_types_count) {}

    const Id& GetId() const noexcept { return id_; }
    size_t GetLootTypesCount() const noexcept { return loot_types_count_; }
    const Roads& GetRoads() const noexcept { return roads_; }
    void AddRoad(const Road& road) { roads_.push_back(road); }

private:
    Id id_;
    std::string name_;
    size_t loot_types_count_;
    Roads roads_;
};

// Исправленная структура для GameSession (фрагмент)
class GameSession {
public:
    using Id = util::Tagged<int, GameSession>;
    
    GameSession(Id id, std::shared_ptr<Map> map, 
                loot_gen::LootGenerator::TimeInterval base_interval, double probability);

    void Update(std::chrono::milliseconds time_delta);
    void AddLooter() { looter_count_++; }
    size_t GetLostObjectsCount() const noexcept { return lost_objects_.size(); }
    
private:
    void GenerateLostObject();
    Point GetRandomRoadPoint() const; // <-- ИСПРАВЛЕНО: добавлен const

    Id id_;
    std::shared_ptr<Map> map_;
    loot_gen::LootGenerator loot_generator_;
    std::unordered_map<int, int> lost_objects_; // Упрощено для примера
    size_t next_lost_object_id_ = 0;
    size_t looter_count_ = 0;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> uniform_dist_;
};

} // namespace model