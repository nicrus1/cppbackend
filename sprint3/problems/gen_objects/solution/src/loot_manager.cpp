#include "loot_manager.h"
#include <algorithm>
#include <random>
#include <cmath>

namespace game {

LootManager::LootManager(const model::Map& map, double period, double probability)
    : map_(map)
    , generator_(
        std::chrono::milliseconds(static_cast<int64_t>(period * 1000)),
        probability,
        [this]() { 
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            return dist(rng_);
        }
    )
    , rng_(std::random_device{}()) {
}

model::Position LootManager::GenerateRandomPosition() const {
    const auto& roads = map_.GetRoads();
    if (roads.empty()) {
        return {0.0, 0.0};
    }
    
    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    const auto& road = roads[road_dist(rng_)];
    
    std::uniform_real_distribution<double> pos_dist(0.0, 1.0);
    double t = pos_dist(rng_);
    
    model::Position result;
    if (road.IsHorizontal()) {
        double x_min = std::min(road.GetStart().x, road.GetEnd().x);
        double x_max = std::max(road.GetStart().x, road.GetEnd().x);
        result.x = x_min + t * (x_max - x_min);
        result.y = static_cast<double>(road.GetStart().y);
    } else {
        double y_min = std::min(road.GetStart().y, road.GetEnd().y);
        double y_max = std::max(road.GetStart().y, road.GetEnd().y);
        result.x = static_cast<double>(road.GetStart().x);
        result.y = y_min + t * (y_max - y_min);
    }
    
    return result;
}

int LootManager::GenerateRandomType() const {
    size_t types_count = map_.GetLootTypesCount();
    if (types_count == 0) {
        return 0;
    }
    std::uniform_int_distribution<int> type_dist(0, types_count - 1);
    return type_dist(rng_);
}

void LootManager::AddLootItems(size_t count) {
    for (size_t i = 0; i < count; ++i) {
        LootItem item;
        item.id = next_id_++;
        item.type = GenerateRandomType();
        item.pos = GenerateRandomPosition();
        loot_items_[item.id] = std::move(item);
    }
}

void LootManager::Update(std::chrono::milliseconds delta, size_t dog_count) {
    size_t current_loot_count = loot_items_.size();
    unsigned new_loot_count = generator_.Generate(delta, current_loot_count, dog_count);
    AddLootItems(new_loot_count);
}

std::unordered_map<uint64_t, std::pair<int, model::Position>> LootManager::GetLootItems() const {
    std::unordered_map<uint64_t, std::pair<int, model::Position>> result;
    for (const auto& [id, item] : loot_items_) {
        result[id] = {item.type, item.pos};
    }
    return result;
}

size_t LootManager::GetLootCount() const {
    return loot_items_.size();
}

} // namespace game