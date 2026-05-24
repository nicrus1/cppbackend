#pragma once
#include "model.h"
#include "players.h"
#include "player_tokens.h"
#include "dog.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <random>

namespace game {

class GameState {
public:
    struct JoinResult {
        model::Token token;
        model::PlayerId player_id;
    };

    struct PlayerState {
        std::string player_id;
        model::Position pos;
        model::Speed speed;
        model::Direction dir;
    };

    explicit GameState(model::Game& game) : game_(game), rng_(std::random_device{}()) {}
    
    JoinResult JoinGame(const std::string& user_name, const model::Map::Id& map_id);
    
    std::unordered_map<std::string, std::string> GetPlayersOnMap(const model::Token& token);
    
    bool ValidateToken(const model::Token& token) const;
    
    // Получить собаку игрока по токену
    const model::Dog* GetDogByToken(const model::Token& token) const;
    
    // Получить собаку игрока по токену (неконстантная версия)
    model::Dog* GetDogByTokenMutable(const model::Token& token);
    
    // Получить состояние игры для конкретного игрока
    std::vector<PlayerState> GetGameState(const model::Token& token) const;
    
    // Изменить направление движения собаки
    void SetDogDirection(const model::Token& token, model::Direction dir);
    
    // Остановить собаку
    void StopDog(const model::Token& token);
    
    // Получить карту игрока по токену
    const model::Map* GetPlayerMap(const model::Token& token) const;
    
    // Для тестов
    std::unordered_map<std::string, std::string> GetPlayersOnMapForTest(const model::Map::Id& map_id) const;

private:
    model::Position GenerateRandomPositionOnMap(const model::Map& map);
    std::optional<model::Road> SelectRandomRoad(const model::Map& map) const;

    model::Game& game_;
    model::Players players_;
    std::unordered_map<model::PlayerId, model::Dog, util::TaggedHasher<model::PlayerId>> dogs_;
    mutable std::mt19937 rng_;
};

} // namespace game