#pragma once

#include "model.h"
#include "players.h"
#include "player_tokens.h"
#include "dog.h"
#include "loot_manager.h"

#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <random>

namespace game {

class GameState {
public:
    static constexpr double ROAD_HALF_WIDTH = 0.4;

    struct JoinResult {
        model::Token token;
        model::PlayerId player_id;
    };

    struct PlayerState {
        std::string player_id;
        model::Position pos;
        model::Speed speed;
        model::Direction dir;
        // ВАЖНО: Добавлено содержимое рюкзака
        std::vector<model::BagItem> bag;
    };

    explicit GameState(model::Game& game);

    // Запрещаем копирование
    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;

    // Разрешаем перемещение
    GameState(GameState&&) = default;
    GameState& operator=(GameState&&) = default;

    JoinResult JoinGame(
        const std::string& user_name,
        const model::Map::Id& map_id);

    std::unordered_map<std::string, std::string>
    GetPlayersOnMap(const model::Token& token) const;

    bool ValidateToken(
        const model::Token& token) const;

    const model::Dog* GetDogByToken(
        const model::Token& token) const;

    model::Dog* GetDogByTokenMutable(
        const model::Token& token);

    std::vector<PlayerState>
    GetGameState(const model::Token& token) const;

    void SetDogDirection(
        const model::Token& token,
        model::Direction dir);

    void StopDog(
        const model::Token& token);

    const model::Map* GetPlayerMap(
        const model::Token& token) const;

    void ProcessTick(int64_t time_delta_ms);

    std::unordered_map<std::string, std::string>
    GetPlayersOnMapForTest(
        const model::Map::Id& map_id) const;

    void SetLootGeneratorConfig(double period, double probability);
    
    void SetLootTypesCount(const model::Map::Id& map_id, size_t count);
    
    std::unordered_map<uint64_t, std::pair<int, model::Position>>
    GetLootState(const model::Token& token) const;

    // Инициализация менеджеров лута для всех карт
    void InitializeLootManagers();

private:
    model::Position GenerateStartPosition(
        const model::Map& map);

    void MoveDog(
        model::Dog& dog,
        const model::Map& map,
        int64_t time_delta_ms);

    void EnsureLootManager(const model::Map& map);

private:
    model::Game& game_;
    model::Players players_;
    std::unordered_map<model::PlayerId, model::Dog, util::TaggedHasher<model::PlayerId>> dogs_;
    std::mt19937 rng_;
    
    double loot_period_ = 5.0;
    double loot_probability_ = 0.5;
    std::unordered_map<model::Map::Id, std::unique_ptr<LootManager>, util::TaggedHasher<model::Map::Id>> loot_managers_;
    bool loot_managers_initialized_ = false;
};

} // namespace game