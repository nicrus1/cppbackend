#pragma once
#include "model.h"
#include "players.h"
#include "player_tokens.h"
#include "dog.h"
#include "road_map.h"
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
    
    const model::Dog* GetDogByToken(const model::Token& token) const;
    model::Dog* GetDogByTokenMutable(const model::Token& token);
    
    std::vector<PlayerState> GetGameState(const model::Token& token) const;
    
    void SetDogDirection(const model::Token& token, model::Direction dir);
    void StopDog(const model::Token& token);
    
    const model::Map* GetPlayerMap(const model::Token& token) const;
    
    void ProcessTick(int64_t time_delta_ms);
    
    std::unordered_map<std::string, std::string> GetPlayersOnMapForTest(const model::Map::Id& map_id) const;

private:
    model::Position GenerateStartPosition(const model::Map& map);
    std::optional<model::Road> SelectFirstRoad(const model::Map& map) const;
    
    void UpdateDogPosition(model::Dog& dog, const model::Map& map, double delta_seconds);
    model::Position MoveDogWithCollision(const model::Dog& dog, const model::Map& map, double delta_seconds) const;
    const model::RoadMap::RoadSegment* FindRoadAt(const model::Map& map, double x, double y) const;
    
    model::Game& game_;
    model::Players players_;
    std::unordered_map<model::PlayerId, model::Dog, util::TaggedHasher<model::PlayerId>> dogs_;
    mutable std::mt19937 rng_;
    
    mutable std::unordered_map<model::Map::Id, model::RoadMap, util::TaggedHasher<model::Map::Id>> road_maps_;
};

} // namespace game