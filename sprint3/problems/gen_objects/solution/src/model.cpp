#include "model.h"
#include <cmath>

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
        time_delta, (unsigned)lost_objects_.size(), (unsigned)looter_count_);
    
    for (unsigned i = 0; i < generated_count; ++i) {
        GenerateLostObject();
    }
}

// Теперь const корректен
model::Point GameSession::GetRandomRoadPoint() const {
    const auto& roads = map_->GetRoads();
    if (roads.empty()) return {0, 0};
    
    size_t road_index = uniform_dist_(rng_) * (roads.size() - 1e-9);
    const auto& road = roads[road_index];
    double t = uniform_dist_(rng_);
    
    if (road.start.x != road.end.x) 
        return {road.start.x + t * (road.end.x - road.start.x), road.start.y};
    else 
        return {road.start.x, road.start.y + t * (road.end.y - road.start.y)};
}

void GameSession::GenerateLostObject() {
    lost_objects_[next_lost_object_id_++] = 0; // Заглушка для теста
}

} // namespace model