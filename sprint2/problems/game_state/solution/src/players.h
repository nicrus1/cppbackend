#pragma once

#include "model.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace app {

using PlayerId = uint32_t;
using Token = std::string;

class Player {
public:
    using Id = PlayerId;

    Player(Id id,
           std::string name,
           const model::Map::Id& map_id)
        : id_(id)
        , name_(std::move(name))
        , map_id_(map_id) {
    }

    Id GetId() const {
        return id_;
    }

    const std::string& GetName() const {
        return name_;
    }

    const model::Map::Id& GetMapId() const {
        return map_id_;
    }

private:
    Id id_;
    std::string name_;
    model::Map::Id map_id_;
};

class Players {
public:
    Player& AddPlayer(std::string name,
                      const model::Map::Id& map_id,
                      Player::Id id) {

        players_.emplace_back(id, std::move(name), map_id);

        return players_.back();
    }

    std::vector<Player*> GetPlayersByMap(
        const model::Map::Id& map_id) {

        std::vector<Player*> result;

        for (auto& player : players_) {
            if (player.GetMapId() == map_id) {
                result.push_back(&player);
            }
        }

        return result;
    }

private:
    std::vector<Player> players_;
};

}  // namespace app