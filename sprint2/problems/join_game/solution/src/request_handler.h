#pragma once
#include "model.h"
#include "players.h"
#include "player_tokens.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <iostream>

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
        
        std::cerr << "DEBUG: JoinGame - Created player " << *player.GetId() 
                  << " with token " << *token << std::endl;
        
        JoinResult result;
        result.token = std::move(token);
        result.player_id = player.GetId();
        return result;
    }
    
    std::unordered_map<std::string, std::string> GetPlayersOnMap(const model::Token& token) {
        std::cerr << "DEBUG: GetPlayersOnMap - Looking for token: " << *token << std::endl;
        
        model::Player* player = players_.FindPlayerByToken(token);
        if (!player) {
            std::cerr << "DEBUG: GetPlayersOnMap - Player not found for token" << std::endl;
            throw std::runtime_error("Invalid token or player not found");
        }
        
        std::cerr << "DEBUG: GetPlayersOnMap - Found player " << *player->GetId() 
                  << " on map " << *player->GetMapId() << std::endl;
        
        auto players_on_map = players_.GetPlayersOnMap(player->GetMapId());
        std::unordered_map<std::string, std::string> result;
        for (auto* p : players_on_map) {
            result[std::to_string(*p->GetId())] = p->GetName();
            std::cerr << "DEBUG: GetPlayersOnMap - Player " << *p->GetId() 
                      << " name " << p->GetName() << std::endl;
        }
        return result;
    }
    
    bool ValidateToken(const model::Token& token) const {
        bool valid = players_.ValidateToken(token);
        std::cerr << "DEBUG: ValidateToken - Token " << *token 
                  << " valid? " << valid << std::endl;
        return valid;
    }

private:
    model::Game& game_;
    model::Players players_;
};

}  // namespace game