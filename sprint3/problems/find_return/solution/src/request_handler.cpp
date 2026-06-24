#include "request_handler.h"
#include <fstream>
#include <sstream>

namespace http_handler {

void RequestHandler::LoadExtraData(const std::filesystem::path& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        logger::LogError(0, "Failed to open config file: " + config_path.string(), "LoadExtraData");
        return;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_str = buffer.str();
    
    try {
        boost::json::value json_value = boost::json::parse(json_str);
        boost::json::object json_obj = json_value.as_object();
        
        auto get_double = [](const boost::json::value& val) -> double {
            if (val.is_double()) return val.as_double();
            if (val.is_int64()) return static_cast<double>(val.as_int64());
            if (val.is_uint64()) return static_cast<double>(val.as_uint64());
            throw std::runtime_error("Not a number");
        };

        // Load loot generator config
        if (json_obj.contains("lootGeneratorConfig")) {
            const auto& config = json_obj.at("lootGeneratorConfig").as_object();
            double period = get_double(config.at("period"));
            double probability = get_double(config.at("probability"));
            extra_data_.SetLootGeneratorConfig(period, probability);
            api_handler_.SetLootGeneratorConfig(period, probability);
            logger::LogDebug("Loot generator config loaded: period=" + std::to_string(period) + 
                           ", probability=" + std::to_string(probability));
        }
        
        // Load loot types for each map
        if (json_obj.contains("maps")) {
            const auto& maps_array = json_obj.at("maps").as_array();
            for (const auto& map_value : maps_array) {
                const auto& map_obj = map_value.as_object();
                std::string map_id = std::string(map_obj.at("id").as_string());
                
                size_t loot_types_count = 0;
                if (map_obj.contains("lootTypes")) {
                    const auto& loot_array = map_obj.at("lootTypes").as_array();
                    loot_types_count = loot_array.size();
                    
                    extra_data::MapExtraData map_data;
                    extra_data::MapExtraData::LootTypes loot_types;
                    for (const auto& loot_value : loot_array) {
                        loot_types.push_back({loot_value.as_object()});
                    }
                    map_data.SetLootTypes(std::move(loot_types));
                    extra_data_.SetMapExtraData(map_id, std::move(map_data));
                }
                
                // Устанавливаем количество типов лута в модели
                const model::Map* map = game_.FindMap(model::Map::Id{map_id});
                if (map) {
                    const_cast<model::Map*>(map)->SetLootTypesCount(loot_types_count);
                    api_handler_.SetLootTypesCount(model::Map::Id{map_id}, loot_types_count);
                    logger::LogDebug("Map " + map_id + " has " + std::to_string(loot_types_count) + 
                                   " loot types");
                }
            }
        }
    } catch (const std::exception& e) {
        logger::LogError(0, "Failed to load extra data: " + std::string(e.what()), "LoadExtraData");
    }
}

std::string RequestHandler::SerializeMaps() const {
    boost::json::array maps_array;
    for (const auto& map : game_.GetMaps()) {
        maps_array.push_back(boost::json::object{
            {"id", *map.GetId()},
            {"name", map.GetName()}
        });
    }
    return boost::json::serialize(maps_array);
}

std::string RequestHandler::SerializeMap(const model::Map& map) const {
    boost::json::object map_obj;
    map_obj["id"] = *map.GetId();
    map_obj["name"] = map.GetName();
    
    // ВОТ ЭТА СТРОКА ИСПРАВЛЯЕТ ОШИБКУ ТЕСТА:
    map_obj["bagCapacity"] = map.GetBagCapacity();
    
    map_obj["roads"] = SerializeRoads(map);
    map_obj["buildings"] = SerializeBuildings(map);
    map_obj["offices"] = SerializeOffices(map);
    
    // Тут у вас также сериализуются lootTypes (из extra_data или из карты)
    // Оставьте вашу текущую логику для lootTypes нетронутой, например:
    if (extra_data_.contains(*map.GetId())) {
        map_obj["lootTypes"] = extra_data_.at(*map.GetId());
    }
    
    return boost::json::serialize(map_obj);
}

boost::json::array RequestHandler::SerializeRoads(const model::Map& map) const {
    boost::json::array roads_array;
    for (const auto& road : map.GetRoads()) {
        boost::json::object road_obj;
        auto start = road.GetStart();
        auto end = road.GetEnd();
        
        if (road.IsHorizontal()) {
            road_obj["x0"] = start.x;
            road_obj["y0"] = start.y;
            road_obj["x1"] = end.x;
        } else {
            road_obj["x0"] = start.x;
            road_obj["y0"] = start.y;
            road_obj["y1"] = end.y;
        }
        roads_array.push_back(road_obj);
    }
    return roads_array;
}

boost::json::array RequestHandler::SerializeBuildings(const model::Map& map) const {
    boost::json::array buildings_array;
    for (const auto& building : map.GetBuildings()) {
        const auto& bounds = building.GetBounds();
        buildings_array.push_back(boost::json::object{
            {"x", bounds.position.x},
            {"y", bounds.position.y},
            {"w", bounds.size.width},
            {"h", bounds.size.height}
        });
    }
    return buildings_array;
}

boost::json::array RequestHandler::SerializeOffices(const model::Map& map) const {
    boost::json::array offices_array;
    for (const auto& office : map.GetOffices()) {
        offices_array.push_back(boost::json::object{
            {"id", *office.GetId()},
            {"x", office.GetPosition().x},
            {"y", office.GetPosition().y},
            {"offsetX", office.GetOffset().dx},
            {"offsetY", office.GetOffset().dy}
        });
    }
    return offices_array;
}

} // namespace http_handler