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
        model::Position pos;
    };
    
    LootManager(const model::Map& map, double period, double probability);
    
    void Update(std::chrono::milliseconds delta, size_t dog_count);
    std::unordered_map<uint64_t, std::pair<int, model::Position>> GetLootItems() const;
    size_t GetLootCount() const;
    
    // Публичный метод для добавления трофеев
    void AddLootItems(size_t count);
    
    // Метод для удаления трофея (при сборе)
    void RemoveLootItem(uint64_t id) {
        loot_items_.erase(id);
    }
    
    // Методы для управления состоянием (сериализация)
    void AddLootItem(uint64_t id, int type, const model::Position& pos) {
        LootItem item;
        item.id = id;
        item.type = type;
        item.pos = pos;
        loot_items_[id] = std::move(item);
        if (id >= next_id_) {
            next_id_ = id + 1;
        }
    }
    
    uint64_t GetNextId() const { return next_id_; }
    void SetNextId(uint64_t id) { next_id_ = id; }
    
    std::chrono::milliseconds GetTimeWithoutLoot() const {
        return generator_.GetTimeWithoutLoot();
    }
    void SetTimeWithoutLoot(std::chrono::milliseconds time) {
        generator_.SetTimeWithoutLoot(time);
    }
    
private:
    const model::Map& map_;
    loot_gen::LootGenerator generator_;
    std::unordered_map<uint64_t, LootItem> loot_items_;
    uint64_t next_id_ = 0;
    std::mt19937 rng_;
    
    model::Position GenerateRandomPosition();
    int GenerateRandomType();
};

} // namespace game