#pragma once

#include "model.h"
#include "loot_generator.h"
#include <unordered_map>
#include <vector>
#include <random>
#include <chrono>
#include <memory>

namespace game {

class LootManager {
public:
    struct LootItem {
        uint64_t id;
        int type;
        int value;  // Добавляем value для подсчета очков
        model::Position pos;
    };
    
    LootManager(const model::Map& map, double period, double probability);
    
    void Update(std::chrono::milliseconds delta, size_t dog_count);
    std::unordered_map<uint64_t, std::pair<int, model::Position>> GetLootItems() const;
    size_t GetLootCount() const;
    
    // Публичный метод для добавления трофеев
    void AddLootItems(size_t count);
    
    // Метод для удаления трофея
    void RemoveLootItem(uint64_t id) {
        loot_items_.erase(id);
    }
    
    // Метод для получения value трофея
    int GetLootValue(uint64_t id) const {
        auto it = loot_items_.find(id);
        if (it != loot_items_.end()) {
            return it->second.value;
        }
        return 0;
    }
    
private:
    const model::Map& map_;
    loot_gen::LootGenerator generator_;
    std::unordered_map<uint64_t, LootItem> loot_items_;
    uint64_t next_id_ = 0;
    std::mt19937 rng_;
    
    model::Position GenerateRandomPosition();
    int GenerateRandomType();
    int GenerateRandomValue();  // Новый метод для генерации value
};

} // namespace game