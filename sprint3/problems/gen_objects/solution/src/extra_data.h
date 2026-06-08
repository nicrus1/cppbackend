#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <boost/json.hpp>

namespace extra_data {

struct LootTypeInfo {
    boost::json::object data;
};

class MapExtraData {
public:
    using LootTypes = std::vector<LootTypeInfo>;
    
    void SetLootTypes(LootTypes loot_types) {
        loot_types_ = std::move(loot_types);
    }
    
    const LootTypes& GetLootTypes() const noexcept {
        return loot_types_;
    }
    
    boost::json::object ToJson() const {
        boost::json::array loot_array;
        for (const auto& loot : loot_types_) {
            loot_array.push_back(loot.data);
        }
        
        boost::json::object result;
        result["lootTypes"] = std::move(loot_array);
        return result;
    }

private:
    LootTypes loot_types_;
};

class ExtraData {
public:
    using MapDataMap = std::unordered_map<std::string, MapExtraData>;
    
    void SetLootGeneratorConfig(double period, double probability) {
        period_ = period;
        probability_ = probability;
    }
    
    double GetLootGeneratorPeriod() const noexcept { return period_; }
    double GetLootGeneratorProbability() const noexcept { return probability_; }
    
    void SetMapExtraData(const std::string& map_id, MapExtraData data) {
        map_extra_data_[map_id] = std::move(data);
    }
    
    const MapExtraData* GetMapExtraData(const std::string& map_id) const {
        auto it = map_extra_data_.find(map_id);
        if (it != map_extra_data_.end()) {
            return &it->second;
        }
        return nullptr;
    }

private:
    double period_ = 5.0;
    double probability_ = 0.5;
    MapDataMap map_extra_data_;
};

} // namespace extra_data