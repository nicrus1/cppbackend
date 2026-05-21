#pragma once
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
        maps_.emplace_back(std::move(map));
    }

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        for (const auto& map : maps_) {
            if (map.GetId() == id) {
                return &map;
            }
        }
        return nullptr;
    }

private:
    Maps maps_;
};

} // namespace model