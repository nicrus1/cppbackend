#pragma once
#include "player.h"
#include "player_tokens.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <algorithm>

namespace model {

class Players {
public:
    Players() = default;

    Players(const PlayerTokens& tokens) = delete;
    Players& operator=(const PlayerTokens& tokens) = delete;

    Player& AddPlayer(std::string name, const Map::Id& map_id) {
        PlayerId new_id{next_id_++};

        auto player = std::make_unique<Player>(
            new_id,
            std::move(name),
            map_id
        );

        Player& ref = *player;

        players_.emplace(new_id, std::move(player));
        map_players_[map_id].push_back(new_id);

        return ref;
    }

    Token GenerateToken(const Player& player) {
        return player_tokens_.GenerateToken(player);
    }

    Player* FindPlayer(PlayerId id) {
        auto it = players_.find(id);

        if (it == players_.end()) {
            return nullptr;
        }

        return it->second.get();
    }

    const Player* FindPlayer(PlayerId id) const {
        auto it = players_.find(id);

        if (it == players_.end()) {
            return nullptr;
        }

        return it->second.get();
    }

    Player* FindPlayerByToken(const Token& token) {
        if (!player_tokens_.IsValidToken(token)) {
            return nullptr;
        }

        PlayerId player_id = player_tokens_.FindPlayerByToken(token);

        return FindPlayer(player_id);
    }

    const Player* FindPlayerByToken(const Token& token) const {
        if (!player_tokens_.IsValidToken(token)) {
            return nullptr;
        }

        PlayerId player_id = player_tokens_.FindPlayerByToken(token);

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

    // Метод для удаления игрока
    bool RemovePlayer(PlayerId id) {
        auto it = players_.find(id);
        if (it == players_.end()) {
            return false;
        }
        
        const Player* player = it->second.get();
        if (player) {
            auto map_it = map_players_.find(player->GetMapId());
            if (map_it != map_players_.end()) {
                auto& vec = map_it->second;
                vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
            }
        }
        
        // Удаляем токен игрока
        // Нам нужно найти токен по id игрока и удалить его
        // Для этого добавим метод в PlayerTokens
        
        players_.erase(it);
        return true;
    }

private:
    uint64_t next_id_ = 0;

    PlayerTokens player_tokens_;

    std::unordered_map<
        PlayerId,
        std::unique_ptr<Player>,
        util::TaggedHasher<PlayerId>
    > players_;

    std::unordered_map<
        Map::Id,
        std::vector<PlayerId>,
        util::TaggedHasher<Map::Id>
    > map_players_;
};

}  // namespace model