#pragma once
#include "model.h"
#include "players.h"
#include "player_tokens.h"
#include <memory>
#include <unordered_map>
#include <string>

namespace game {

class GameSession {
public:
    explicit GameSession(model::Game& game)
        : game_(game) {
    }
    
    struct JoinResult {
        model::Token token;
        model::PlayerId player_id;
    };
    
    JoinResult JoinGame(const std::string& user_name, const model::Map::Id& map_id) {
        const model::Map* map = game_.FindMap(map_id);
        if (!map) {
            throw std::runtime_error("Map not found");
        }
        
        model::Player& player = players_.AddPlayer(user_name, map_id);
        model::Token token = players_.GenerateToken(player);
        
        JoinResult result;
        result.token = std::move(token);
        result.player_id = player.GetId();
        return result;
    }
    
    std::unordered_map<std::string, std::string> GetPlayersOnMap(const model::Token& token) {
        model::Player* player = players_.FindPlayerByToken(token);
        if (!player) {
            throw std::runtime_error("Invalid token or player not found");
        }
        
        auto players_on_map = players_.GetPlayersOnMap(player->GetMapId());
        std::unordered_map<std::string, std::string> result;
        for (auto* p : players_on_map) {
            result[std::to_string(*p->GetId())] = p->GetName();
        }
        return result;
    }
    
    // Метод для тестов - получить всех игроков на карте без проверки токена
    std::unordered_map<std::string, std::string> GetPlayersOnMapForTest(const model::Map::Id& map_id) {
        auto players_on_map = players_.GetPlayersOnMap(map_id);
        std::unordered_map<std::string, std::string> result;
        for (auto* p : players_on_map) {
            result[std::to_string(*p->GetId())] = p->GetName();
        }
        return result;
    }
    
    bool ValidateToken(const model::Token& token) const {
        return players_.ValidateToken(token);
    }

private:
    model::Game& game_;
    model::Players players_;
};

}  // namespace game