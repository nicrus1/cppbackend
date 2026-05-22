#pragma once

#include "model.h"
#include "players.h"

#include <unordered_map>

namespace game {

using PlayerId = uint32_t;

struct JoinResult {
    model::Token token;
    PlayerId player_id;
};

class GameState {
public:
    explicit GameState(model::Game& game)
        : game_(game) {
    }

    JoinResult JoinGame(const std::string& user_name,
                        const model::Map::Id& map_id);

    bool ValidateToken(const model::Token& token) const;

    std::unordered_map<std::string, std::string>
    GetPlayersOnMap(const model::Token& token) const;

    std::unordered_map<PlayerId, app::Player*>
    GetGameState(const model::Token& token);

private:
    model::Game& game_;
    app::Players players_;
};

} // namespace game