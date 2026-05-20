#pragma once
#include "player.h"
#include <unordered_map>
#include <memory>
#include <vector>

namespace model {

class Players {
public:
    Player& AddPlayer(std::string name, const Map::Id& map_id) {
        PlayerId new_id{next_id_++};
        auto player = std::make_unique<Player>(new_id, std::move(name), map_id);
        Player& ref = *player;
        players_[new_id] = std::move(player);
        map_players_[map_id].push_back(new_id);
        return ref;
    }
    
    Player* FindPlayer(PlayerId id) {
        auto it = players_.find(id);
        return it != players_.end() ? it->second.get() : nullptr;
    }
    
    const Player* FindPlayer(PlayerId id) const {
        auto it = players_.find(id);
        return it != players_.end() ? it->second.get() : nullptr;
    }
    
    std::vector<Player*> GetPlayersOnMap(const Map::Id& map_id) {
        std::vector<Player*> result;
        auto it = map_players_.find(map_id);
        if (it != map_players_.end()) {
            for (auto player_id : it->second) {
                if (auto* player = FindPlayer(player_id)) {
                    result.push_back(player);
                }
            }
        }
        return result;
    }

private:
    uint64_t next_id_{0};
    std::unordered_map<PlayerId, std::unique_ptr<Player>, util::TaggedHasher<PlayerId>> players_;
    std::unordered_map<Map::Id, std::vector<PlayerId>, util::TaggedHasher<Map::Id>> map_players_;
};

}  // namespace model