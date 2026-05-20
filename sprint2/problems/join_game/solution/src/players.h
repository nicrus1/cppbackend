#pragma once
#include "player.h"
#include "player_tokens.h"
#include <unordered_map>
#include <memory>
#include <vector>

namespace model {

class Players {
public:
    Players() = default;
    
    explicit Players(PlayerTokens& tokens)
        : player_tokens_(tokens)
    {
    }
    
    Player& AddPlayer(std::string name, const Map::Id& map_id) {
        PlayerId new_id{next_id_++};
        auto player = std::make_unique<Player>(new_id, std::move(name), map_id);
        Player& ref = *player;
        players_[new_id] = std::move(player);
        map_players_[map_id].push_back(new_id);
        return ref;
    }
    
    Token GenerateToken(const Player& player) {
        return player_tokens_.GenerateToken(player);
    }
    
    Player* FindPlayer(PlayerId id) {
        auto it = players_.find(id);
        return it != players_.end() ? it->second.get() : nullptr;
    }
    
    const Player* FindPlayer(PlayerId id) const {
        auto it = players_.find(id);
        return it != players_.end() ? it->second.get() : nullptr;
    }
    
    Player* FindPlayerByToken(const Token& token) {
        PlayerId player_id = player_tokens_.FindPlayerByToken(token);
        if (player_id == PlayerId{0}) {
            return nullptr;
        }
        return FindPlayer(player_id);
    }
    
    const Player* FindPlayerByToken(const Token& token) const {
        PlayerId player_id = player_tokens_.FindPlayerByToken(token);
        if (player_id == PlayerId{0}) {
            return nullptr;
        }
        return FindPlayer(player_id);
    }
    
    bool ValidateToken(const Token& token) const {
        return player_tokens_.IsValidToken(token);
    }
    
    std::vector<Player*> GetPlayersOnMap(const Map::Id& map_id) {
        std::vector<Player*> result;
        auto it = map_players_.find(map_id);
        if (it != map_players_.end()) {
            for (auto player_id : it->second) {
                Player* player = FindPlayer(player_id);
                if (player) {
                    result.push_back(player);
                }
            }
        }
        return result;
    }
    
    std::vector<const Player*> GetPlayersOnMap(const Map::Id& map_id) const {
        std::vector<const Player*> result;
        auto it = map_players_.find(map_id);
        if (it != map_players_.end()) {
            for (auto player_id : it->second) {
                const Player* player = FindPlayer(player_id);
                if (player) {
                    result.push_back(player);
                }
            }
        }
        return result;
    }
    
    bool HasPlayer(PlayerId id) const {
        return players_.find(id) != players_.end();
    }

private:
    uint64_t next_id_{0};
    PlayerTokens player_tokens_;
    std::unordered_map<PlayerId, std::unique_ptr<Player>, util::TaggedHasher<PlayerId>> players_;
    std::unordered_map<Map::Id, std::vector<PlayerId>, util::TaggedHasher<Map::Id>> map_players_;
};

}  // namespace model