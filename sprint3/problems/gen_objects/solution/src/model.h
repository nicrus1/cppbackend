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

struct Player {
    size_t id;
    std::string name;
    Point position;
    std::vector<double> speed = {0.0, 0.0};
    std::string dir = "U"; // 'U', 'D', 'L', 'R'
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
        : id_(std::move(id))
        , name_(std::move(name))
        , loot_types_count_(loot_types_count) {}

    const Id& GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }
    size_t GetLootTypesCount() const noexcept { return loot_types_count_; }

    void AddRoad(const Road& road) { roads_.push_back(road); }
    void AddBuilding(const Building& building) { buildings_.push_back(building); }
    void AddOffice(const Office& office) { offices_.push_back(office); }

    const Roads& GetRoads() const noexcept { return roads_; }
    const Buildings& GetBuildings() const noexcept { return buildings_; }
    const Offices& GetOffices() const noexcept { return offices_; }

 private:
    Id id_;
    std::string name_;
    size_t loot_types_count_;
    Roads roads_;
    Buildings buildings_;
    Offices offices_;
};

struct LostObject {
    using Id = util::Tagged<size_t, LostObject>;
    Id id;
    size_t type;
    Point position;
};

class GameSession {
public:
    using Id = util::Tagged<size_t, GameSession>;

    // Добавлен опциональный параметр random_gen для детерминированного тестирования
    GameSession(Id id, std::shared_ptr<Map> map,
                loot_gen::LootGenerator::TimeInterval base_interval,
                double probability,
                loot_gen::LootGenerator::RandomGenerator random_gen = nullptr);

    const Id& GetId() const noexcept { return id_; }
    std::shared_ptr<Map> GetMap() const noexcept { return map_; }
    
    void Update(std::chrono::milliseconds time_delta);

    void AddLooter() { looter_count_++; }
    void RemoveLooter() { if (looter_count_ > 0) looter_count_--; }
    
    size_t GetLooterCount() const noexcept { return looter_count_; }
    size_t GetLostObjectsCount() const noexcept { return lost_objects_.size(); }
    const std::unordered_map<LostObject::Id, LostObject>& GetLostObjects() const noexcept { return lost_objects_; }
    Player* AddPlayer(const std::string& name);
    const std::unordered_map<size_t, Player>& GetPlayers() const noexcept { return players_; }

private:
    void GenerateLostObject();
    Point GetRandomRoadPoint();

    Id id_;
    std::shared_ptr<Map> map_;
    loot_gen::LootGenerator loot_generator_;
    std::unordered_map<LostObject::Id, LostObject> lost_objects_;
    size_t next_lost_object_id_ = 0;
    size_t looter_count_ = 0;
    
    std::mt19937 rng_;
    std::uniform_real_distribution<double> uniform_dist_;

    std::unordered_map<size_t, Player> players_;
    size_t next_player_id_ = 0;
};

class Game {
public:
    using Maps = std::unordered_map<Map::Id, std::shared_ptr<Map>>;
    using Sessions = std::unordered_map<GameSession::Id, std::shared_ptr<GameSession>>;

    void AddMap(std::shared_ptr<Map> map);
    std::shared_ptr<Map> FindMap(const Map::Id& id) const noexcept;
    const Maps& GetMaps() const noexcept { return maps_; }
    
    void AddSession(std::shared_ptr<GameSession> session);
    const Sessions& GetSessions() const noexcept { return sessions_; }
    
    void Update(std::chrono::milliseconds time_delta);

private:
    Maps maps_;
    Sessions sessions_;
};

} // namespace model