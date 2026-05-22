#pragma once

#include "model.h"
#include "players.h"

#include <unordered_map>
#include <string>

namespace game {

struct JoinResult {
    app::Player::Id player_id;
    app::Token token;
};

class GameState {
public:
    explicit GameState(model::Game& game)
        : game_(game) {
    }

    JoinResult JoinGame(const std::string& user_name,
                        const model::Map::Id& map_id);

    bool ValidateToken(const app::Token& token) const;

    std::unordered_map<std::string, std::string>
    GetPlayersOnMap(const app::Token& token) const;

    std::unordered_map<std::string, app::Player*>
    GetGameState(const app::Token& token);

private:
    model::Game& game_;
    app::Players players_;

    std::unordered_map<app::Token, app::Player*> token_to_player_;

    size_t next_player_id_ = 0;
};

}  // namespace game