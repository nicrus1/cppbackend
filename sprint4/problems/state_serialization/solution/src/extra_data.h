#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <boost/json.hpp>

namespace extra_data {

struct LootType {
    boost::json::object data;
    
    explicit LootType(const boost::json::object& obj) : data(obj) {}
};

class MapExtraData {
public:
    using LootTypes = std::vector<LootType>;
    
    void SetLootTypes(LootTypes&& types) {
        loot_types_ = std::move(types);
    }
    
    const LootTypes& GetLootTypes() const {
        return loot_types_;
    }
    
private:
    LootTypes loot_types_;
};

class ExtraData {
public:
    void SetLootGeneratorConfig(double period, double probability) {
        loot_period_ = period;
        loot_probability_ = probability;
    }
    
    void SetMapExtraData(const std::string& map_id, MapExtraData&& data) {
        map_data_[map_id] = std::move(data);
    }
    
    const MapExtraData* GetMapExtraData(const std::string& map_id) const {
        auto it = map_data_.find(map_id);
        if (it == map_data_.end()) {
            return nullptr;
        }
        return &it->second;
    }
    
    double GetLootPeriod() const { return loot_period_; }
    double GetLootProbability() const { return loot_probability_; }
    
private:
    double loot_period_ = 5.0;
    double loot_probability_ = 0.5;
    std::unordered_map<std::string, MapExtraData> map_data_;
};

} // namespace extra_data