#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstdint>

#include <boost/json.hpp>
#include <boost/asio.hpp>

#include "model.h"
#include "extra_data.h"
#include "loot_generator.h"

namespace json = boost::json;

class GameServer {
public:
    GameServer(const std::string& config_path) {
        LoadConfig(config_path);
    }
    
    void Run() {
        std::cout << "Server running. Press Ctrl+C to stop.\\n";
        
        auto last_time = std::chrono::steady_clock::now();
        
        while (running_) {
            auto now = std::chrono::steady_clock::now();
            auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time);
            
            if (delta.count() > 0) {
                game_.Update(delta);
                last_time = now;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    json::value HandleMapRequest(const std::string& map_id) {
        auto map = game_.FindMap(model::Map::Id(map_id));
        if (!map) {
            return json::value{nullptr};
        }
        
        json::object map_json;
        map_json["id"] = map->GetId().GetUnderlying();
        map_json["name"] = map->GetName();
        
        // Roads
        json::array roads_json;
        for (const auto& road : map->GetRoads()) {
            json::object road_obj;
            road_obj["x0"] = road.start.x;
            road_obj["y0"] = road.start.y;
            if (road.start.x != road.end.x) {
                road_obj["x1"] = road.end.x;
            } else {
                road_obj["y1"] = road.end.y;
            }
            roads_json.push_back(road_obj);
        }
        map_json["roads"] = roads_json;
        
        // Добавляем lootTypes из extra_data слоя
        auto extra_data = extra_data_.GetMapExtraData(map_id);
        if (extra_data) {
            map_json["lootTypes"] = extra_data->ToJson()["lootTypes"];
        } else {
            map_json["lootTypes"] = json::array{};
        }
        
        return map_json;
    }
    
    json::value HandleGameStateRequest() {
        json::object state_json;
        
        // В рамках этого задания отдаем пустой список игроков, либо базовую структуру
        state_json["players"] = json::object{};
        
        json::object lost_objects;
        for (const auto& [session_id, session] : game_.GetSessions()) {
            for (const auto& [obj_id, obj] : session->GetLostObjects()) {
                json::object obj_json;
                obj_json["type"] = static_cast<std::int64_t>(obj.type);
                
                json::array pos;
                pos.push_back(obj.position.x);
                pos.push_back(obj.position.y);
                obj_json["pos"] = pos;
                
                lost_objects[std::to_string(obj_id.GetUnderlying())] = std::move(obj_json);
            }
        }
        state_json["lostObjects"] = lost_objects;
        
        return state_json;
    }

private:
    void LoadConfig(const std::string& config_path) {
        std::ifstream f(config_path);
        if (!f.is_open()) {
            throw std::runtime_error("Failed to open config file");
        }
        
        std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto config = json::parse(str);
        
        // Parse global loot generator config
        if (config.as_object().contains("lootGeneratorConfig")) {
            auto loot_config = config.at("lootGeneratorConfig").as_object();
            extra_data_.SetLootGeneratorConfig(
                loot_config.at("period").as_double(),
                loot_config.at("probability").as_double()
            );
        } else {
            // Default fallback
            extra_data_.SetLootGeneratorConfig(1.0, 1.0);
        }
        
        // Parse maps
        if (config.as_object().contains("maps")) {
            for (const auto& map_val : config.at("maps").as_array()) {
                auto map_obj = map_val.as_object();
                std::string id = map_obj.at("id").as_string().c_str();
                std::string name = map_obj.at("name").as_string().c_str();
                
                size_t loot_types_count = 0;
                extra_data::MapExtraData map_extra;
                if (map_obj.contains("lootTypes")) {
                    auto loot_types_arr = map_obj.at("lootTypes").as_array();
                    loot_types_count = loot_types_arr.size();
                    
                    extra_data::MapExtraData::LootTypes loot_types;
                    for (const auto& loot_val : loot_types_arr) {
                        loot_types.push_back({loot_val.as_object()});
                    }
                    map_extra.SetLootTypes(std::move(loot_types));
                }
                
                auto map = std::make_shared<model::Map>(model::Map::Id(id), name, loot_types_count);
                
                // Load roads
                if (map_obj.contains("roads")) {
                    for (const auto& road_val : map_obj.at("roads").as_array()) {
                        auto road_obj = road_val.as_object();
                        double x0 = road_obj.at("x0").as_double();
                        double y0 = road_obj.at("y0").as_double();
                        double x1 = road_obj.contains("x1") ? road_obj.at("x1").as_double() : x0;
                        double y1 = road_obj.contains("y1") ? road_obj.at("y1").as_double() : y0;
                        map->AddRoad({{x0, y0}, {x1, y1}});
                    }
                }
                
                // Load offices
                if (map_obj.contains("offices")) {
                    for (const auto& office_val : map_obj.at("offices").as_array()) {
                        auto office_obj = office_val.as_object();
                        model::Office office{
                            model::Office::Id(office_obj.at("id").as_string().c_str()),
                            {office_obj.at("x").as_double(), office_obj.at("y").as_double()},
                            {office_obj.at("offsetX").as_double(), office_obj.at("offsetY").as_double()}
                        };
                        map->AddOffice(std::move(office));
                    }
                }
                
                game_.AddMap(map);
                extra_data_.SetMapExtraData(id, std::move(map_extra));
                
                // Create game session for this map
                auto period_ms = std::chrono::milliseconds(
                    static_cast<long long>(extra_data_.GetLootGeneratorPeriod() * 1000)
                );
                auto session = std::make_shared<model::GameSession>(\
                    model::GameSession::Id(next_session_id_++),
                    map,
                    period_ms,
                    extra_data_.GetLootGeneratorProbability()
                );
                game_.AddSession(session);
            }
        }
    }
    
    model::Game game_;
    extra_data::ExtraData extra_data_;
    bool running_ = true;
    size_t next_session_id_ = 0;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config.json>\\n";
        return 1;
    }
    
    try {
        GameServer server(argv[1]);
        server.Run();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\\n";
        return 1;
    }
    
    return 0;
}