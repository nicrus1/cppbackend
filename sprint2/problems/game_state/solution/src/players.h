#pragma once

#include <string>
#include <vector>

namespace app {

using PlayerId = uint32_t;

class Player {
public:
    Player(PlayerId id, std::string name, const model::Map::Id& map_id)
        : id_(id)
        , name_(std::move(name))
        , map_id_(map_id) {
    }

    PlayerId GetId() const {
        return id_;
    }

    const std::string& GetName() const {
        return name_;
    }

    const model::Map::Id& GetMapId() const {
        return map_id_;
    }

    const model::Position& GetPosition() const {
        return position_;
    }

    const model::Speed& GetSpeed() const {
        return speed_;
    }

    model::Direction GetDirection() const {
        return direction_;
    }

    void SetPosition(model::Position position) {
        position_ = position;
    }

    void SetSpeed(model::Speed speed) {
        speed_ = speed;
    }

    void SetDirection(model::Direction direction) {
        direction_ = direction;
    }

private:
    PlayerId id_;
    std::string name_;
    model::Map::Id map_id_;

    model::Position position_;
    model::Speed speed_;
    model::Direction direction_ = model::Direction::NORTH;
};

class Players {
public:
    Player& AddPlayer(std::string name, const model::Map::Id& map_id) {
        players_.emplace_back(next_id_++, std::move(name), map_id);
        return players_.back();
    }

    Player* FindById(PlayerId id) {
        for (auto& player : players_) {
            if (player.GetId() == id) {
                return &player;
            }
        }
        return nullptr;
    }

    std::vector<Player*> GetPlayersByMap(const model::Map::Id& map_id) {
        std::vector<Player*> result;

        for (auto& player : players_) {
            if (player.GetMapId() == map_id) {
                result.push_back(&player);
            }
        }

        return result;
    }

private:
    PlayerId next_id_ = 0;
    std::vector<Player> players_;
};

} // namespace app