#pragma once
#include "model.h"
#include "players.h"
#include "player_tokens.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>

namespace game {

class GameState {
public:
    struct JoinResult {
        model::Token token;
        model::PlayerId player_id;
    };

    explicit GameState(model::Game& game) : game_(game) {}
    
    JoinResult JoinGame(const std::string& user_name, const model::Map::Id& map_id);
    
    std::unordered_map<std::string, std::string> GetPlayersOnMap(const model::Token& token);
    
    bool ValidateToken(const model::Token& token) const;
    
    // Для тестов
    std::unordered_map<std::string, std::string> GetPlayersOnMapForTest(const model::Map::Id& map_id) const;

private:
    model::Game& game_;
    model::Players players_;
};

} // namespace game