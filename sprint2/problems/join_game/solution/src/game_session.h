#pragma once
#include "model.h"
#include "players.h"
#include "player_tokens.h"
#include <memory>

namespace game {

class GameSession {
public:
    explicit GameSession(model::Game& game)
        : game_(game) {}
    
    // Вход в игру
    struct JoinResult {
        model::Token token;
        model::PlayerId player_id;
    };
    
    JoinResult JoinGame(const std::string& user_name, const model::Map::Id& map_id) {
        // Проверка существования карты
        if (!game_.FindMap(map_id)) {
            throw std::runtime_error("Map not found");
        }
        
        // Создание игрока
        model::Player& player = players_.AddPlayer(user_name, map_id);
        
        // Генерация токена
        model::Token token = player_tokens_.GenerateToken(player);
        
        return {std::move(token), player.GetId()};
    }
    
    // Получение списка игроков на карте игрока
    std::unordered_map<std::string, std::string> GetPlayersOnMap(const model::Token& token) {
        model::PlayerId player_id = player_tokens_.FindPlayerByToken(token);
        if (player_id == model::PlayerId{0}) {
            throw std::runtime_error("Invalid token");
        }
        
        model::Player* player = players_.FindPlayer(player_id);
        if (!player) {
            throw std::runtime_error("Player not found");
        }
        
        auto players_on_map = players_.GetPlayersOnMap(player->GetMapId());
        std::unordered_map<std::string, std::string> result;
        for (auto* p : players_on_map) {
            result[std::to_string(*p->GetId())] = p->GetName();
        }
        return result;
    }
    
    // Валидация токена
    bool ValidateToken(const model::Token& token) const {
        return player_tokens_.IsValidToken(token);
    }

private:
    model::Game& game_;
    model::Players players_;
    model::PlayerTokens player_tokens_;
};

}  // namespace game