#include "game_state.h"

#include <cmath>

namespace game {

GameState::JoinResult GameState::JoinGame(
    const std::string& user_name,
    const model::Map::Id& map_id) {

    const model::Map* map = game_.FindMap(map_id);

    if (!map) {
        throw std::runtime_error("Map not found");
    }

    model::Player& player =
        players_.AddPlayer(user_name, map_id);

    model::Token token =
        players_.GenerateToken(player);

    const auto& first_road = map->GetRoads().front();
    auto start = first_road.GetStart();

    model::Position start_pos{
        static_cast<double>(start.x),
        static_cast<double>(start.y)
    };

    uint64_t dog_id = *player.GetId();

    model::Dog dog(dog_id, start_pos);

    dogs_.emplace(player.GetId(), std::move(dog));

    player.SetDogId(dog_id);

    return {
        token,
        player.GetId()
    };
}

const model::Dog* GameState::GetDogByToken(
    const model::Token& token) const {

    const model::Player* player =
        players_.FindPlayerByToken(token);

    if (!player) {
        return nullptr;
    }

    auto it = dogs_.find(player->GetId());

    if (it == dogs_.end()) {
        return nullptr;
    }

    return &it->second;
}

model::Dog* GameState::GetDogByTokenMutable(
    const model::Token& token) {

    model::Player* player =
        players_.FindPlayerByToken(token);

    if (!player) {
        return nullptr;
    }

    auto it = dogs_.find(player->GetId());

    if (it == dogs_.end()) {
        return nullptr;
    }

    return &it->second;
}

const model::Map* GameState::GetPlayerMap(
    const model::Token& token) const {

    const model::Player* player =
        players_.FindPlayerByToken(token);

    if (!player) {
        return nullptr;
    }

    return game_.FindMap(player->GetMapId());
}

void GameState::SetDogDirection(
    const model::Token& token,
    model::Direction dir) {

    model::Dog* dog =
        GetDogByTokenMutable(token);

    if (!dog) {
        throw std::runtime_error("Dog not found");
    }

    const model::Map* map =
        GetPlayerMap(token);

    if (!map) {
        throw std::runtime_error("Map not found");
    }

    dog->SetSpeedFromDirection(
        dir,
        map->GetDogSpeed()
    );
}

void GameState::StopDog(
    const model::Token& token) {

    model::Dog* dog =
        GetDogByTokenMutable(token);

    if (!dog) {
        throw std::runtime_error("Dog not found");
    }

    dog->Stop();
}

void GameState::Update(uint64_t time_ms) {
    double dt = time_ms / 1000.0;

    for (auto& [id, dog] : dogs_) {
        auto pos = dog.GetPosition();
        auto speed = dog.GetSpeed();

        if (speed.vx == 0.0 && speed.vy == 0.0) {
            continue;
        }

        double new_x = pos.x + speed.vx * dt;
        double new_y = pos.y + speed.vy * dt;

        model::Player* player = players_.FindPlayer(id);
        if (!player) {
            continue;
        }

        const model::Map* map = game_.FindMap(player->GetMapId());
        if (!map) {
            continue;
        }

        // Check if the new position is on any road
        bool on_road = false;
        const model::Road* current_road = nullptr;
        
        for (const auto& road : map->GetRoads()) {
            auto start = road.GetStart();
            auto end = road.GetEnd();
            
            if (road.IsHorizontal()) {
                // Check if Y is within road width (0.4 units)
                if (std::abs(new_y - start.y) <= 0.4 + 1e-9) {
                    double min_x = std::min(start.x, end.x);
                    double max_x = std::max(start.x, end.x);
                    if (new_x >= min_x - 0.4 - 1e-9 && new_x <= max_x + 0.4 + 1e-9) {
                        on_road = true;
                        current_road = &road;
                        break;
                    }
                }
            } else {
                // Check if X is within road width (0.4 units)
                if (std::abs(new_x - start.x) <= 0.4 + 1e-9) {
                    double min_y = std::min(start.y, end.y);
                    double max_y = std::max(start.y, end.y);
                    if (new_y >= min_y - 0.4 - 1e-9 && new_y <= max_y + 0.4 + 1e-9) {
                        on_road = true;
                        current_road = &road;
                        break;
                    }
                }
            }
        }
        
        if (on_road) {
            // Movement is valid - update position
            dog.SetPosition({new_x, new_y});
        } else {
            // Hit the edge of the road - stop the dog
            dog.Stop();
            
            // Try to find where we should snap to
            for (const auto& road : map->GetRoads()) {
                auto start = road.GetStart();
                auto end = road.GetEnd();
                
                if (road.IsHorizontal()) {
                    if (std::abs(pos.y - start.y) <= 0.4 + 1e-9) {
                        double min_x = std::min(start.x, end.x);
                        double max_x = std::max(start.x, end.x);
                        
                        if (speed.vx < 0 && new_x < min_x - 0.4) {
                            new_x = min_x - 0.4;
                            dog.SetPosition({new_x, pos.y});
                        } else if (speed.vx > 0 && new_x > max_x + 0.4) {
                            new_x = max_x + 0.4;
                            dog.SetPosition({new_x, pos.y});
                        }
                        break;
                    }
                } else {
                    if (std::abs(pos.x - start.x) <= 0.4 + 1e-9) {
                        double min_y = std::min(start.y, end.y);
                        double max_y = std::max(start.y, end.y);
                        
                        if (speed.vy < 0 && new_y < min_y - 0.4) {
                            new_y = min_y - 0.4;
                            dog.SetPosition({pos.x, new_y});
                        } else if (speed.vy > 0 && new_y > max_y + 0.4) {
                            new_y = max_y + 0.4;
                            dog.SetPosition({pos.x, new_y});
                        }
                        break;
                    }
                }
            }
        }
    }
}

std::vector<GameState::PlayerState>
GameState::GetGameState(
    const model::Token& token) const {

    const model::Player* player =
        players_.FindPlayerByToken(token);

    if (!player) {
        return {};
    }

    std::vector<PlayerState> result;

    auto players_on_map =
        players_.GetPlayersOnMap(player->GetMapId());

    for (const auto* p : players_on_map) {

        auto it = dogs_.find(p->GetId());

        if (it == dogs_.end()) {
            continue;
        }

        const auto& d = it->second;

        result.push_back({
            std::to_string(*p->GetId()),
            d.GetPosition(),
            d.GetSpeed(),
            d.GetDirection()
        });
    }

    return result;
}

bool GameState::ValidateToken(
    const model::Token& token) const {

    return players_.ValidateToken(token);
}

std::unordered_map<std::string, std::string>
GameState::GetPlayersOnMap(
    const model::Token& token) {

    model::Player* player =
        players_.FindPlayerByToken(token);

    if (!player) {
        throw std::runtime_error("Invalid token");
    }

    auto players_on_map =
        players_.GetPlayersOnMap(player->GetMapId());

    std::unordered_map<std::string, std::string> result;

    for (auto* p : players_on_map) {
        result[std::to_string(*p->GetId())] =
            p->GetName();
    }

    return result;
}

} // namespace game