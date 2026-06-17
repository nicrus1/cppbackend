#include "model.h"

#include <cmath>
#include <random>

namespace model {

GameSession::GameSession(Id id, std::shared_ptr<Map> map,
                        loot_gen::LootGenerator::TimeInterval base_interval,
                        double probability)
    : id_(std::move(id))
    , map_(std::move(map))
    , loot_generator_(base_interval, probability, [this]() { return uniform_dist_(rng_); })
    , rng_(std::random_device{}())
    , uniform_dist_(0.0, 1.0) {}

void GameSession::Update(std::chrono::milliseconds time_delta) {
    unsigned generated_count = loot_generator_.Generate(
        time_delta, 
        static_cast<unsigned>(lost_objects_.size()),
        static_cast<unsigned>(looter_count_)
    );
    
    for (unsigned i = 0; i < generated_count; ++i) {
        GenerateLostObject();
    }
}

void GameSession::GenerateLostObject() {
    LostObject::Id id(next_lost_object_id_++);
    
    // Случайный тип от 0 до N-1
    size_t type = uniform_dist_(rng_) * (map_->GetLootTypesCount() - 1e-9);
    
    LostObject obj{
        .id = id,
        .type = type,
        .position = GetRandomRoadPoint()
    };
    
    lost_objects_.emplace(id, std::move(obj));
}

// Убран const
Point GameSession::GetRandomRoadPoint() {
    const auto& roads = map_->GetRoads();
    if (roads.empty()) {
        return {0, 0};
    }
    
    // Выбираем случайную дорогу
    size_t road_index = uniform_dist_(rng_) * (roads.size() - 1e-9);
    const auto& road = roads[road_index];
    
    // Генерируем точку на дороге
    double t = uniform_dist_(rng_);
    
    double x, y;
    if (road.start.x != road.end.x) {
        // Горизонтальная дорога
        x = road.start.x + t * (road.end.x - road.start.x);
        y = road.start.y;
    } else {
        // Вертикальная дорога
        x = road.start.x;
        y = road.start.y + t * (road.end.y - road.start.y);
    }
    
    return {x, y};
}

void Game::AddMap(std::shared_ptr<Map> map) {
    maps_.emplace(map->GetId(), std::move(map));
}

std::shared_ptr<Map> Game::FindMap(const Map::Id& id) const noexcept {
    auto it = maps_.find(id);
    if (it != maps_.end()) {
        return it->second;
    }
    return nullptr;
}

void Game::AddSession(std::shared_ptr<GameSession> session) {
    sessions_.emplace(session->GetId(), std::move(session));
}

std::shared_ptr<GameSession> Game::FindSession(const GameSession::Id& id) const noexcept {
    auto it = sessions_.find(id);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

void Game::Update(std::chrono::milliseconds time_delta) {
    for (auto& [id, session] : sessions_) {
        session->Update(time_delta);
    }
}

} // namespace model