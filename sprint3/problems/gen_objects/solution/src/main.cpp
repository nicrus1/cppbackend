#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

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
        std::cout << "Server running. Press Ctrl+C to stop.\n";
        
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
        
        json::object result;
        result["id"] = map->GetId().GetUnderlying();
        result["name"] = map->GetName();
        
        // Add roads
        json::array roads;
        for (const auto& road : map->GetRoads()) {
            json::object r;
            r["x0"] = road.start.x;
            r["y0"] = road.start.y;
            if (road.start.x != road.end.x) {
                r["x1"] = road.end.x;
            } else {
                r["y1"] = road.end.y;
            }
            roads.push_back(std::move(r));
        }
        result["roads"] = std::move(roads);
        
        // Add buildings
        json::array buildings;
        for (const auto& building : map->GetBuildings()) {
            json::object b;
            b["x"] = building.bounds.position.x;
            b["y"] = building.bounds.position.y;
            b["w"] = building.bounds.size.width;
            b["h"] = building.bounds.size.height;
            buildings.push_back(std::move(b));
        }
        result["buildings"] = std::move(buildings);
        
        // Add offices
        json::array offices;
        for (const auto& office : map->GetOffices()) {
            json::object o;
            o["id"] = office.id.GetUnderlying();
            o["x"] = office.position.x;
            o["y"] = office.position.y;
            o["offsetX"] = office.offset.dx;
            o["offsetY"] = office.offset.dy;
            offices.push_back(std::move(o));
        }
        result["offices"] = std::move(offices);
        
        // Add loot types from extra data
        auto extra = extra_data_.GetMapExtraData(map_id);
        if (extra) {
            auto loot_json = extra->ToJson();
            for (const auto& [key, value] : loot_json) {
                result[key] = value;
            }
        }
        
        return result;
    }
    
    json::value HandleGameStateRequest() {
        json::object result;
        
        // Players (simplified for now)
        json::object players;
        result["players"] = std::move(players);
        
        // Lost objects
        json::object lost_objects;
        for (const auto& [session_id, session] : game_.GetSessions()) {
            for (const auto& [obj_id, obj] : session->GetLostObjects()) {
                json::object obj_json;
                obj_json["type"] = static_cast<json::int64>(obj.type);
                obj_json["pos"] = json::array{obj.position.x, obj.position.y};
                lost_objects[std::to_string(*obj_id)] = std::move(obj_json);
            }
        }
        result["lostObjects"] = std::move(lost_objects);
        
        return result;
    }

private:
    void LoadConfig(const std::string& config_path) {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open config file: " + config_path);
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        
        auto config = json::parse(content).as_object();
        
        // Load loot generator config
        if (config.contains("lootGeneratorConfig")) {
            auto& loot_config = config["lootGeneratorConfig"].as_object();
            double period = loot_config["period"].as_double();
            double probability = loot_config["probability"].as_double();
            extra_data_.SetLootGeneratorConfig(period, probability);
        }
        
        // Load maps
        if (config.contains("maps")) {
            auto& maps_array = config["maps"].as_array();
            for (const auto& map_json : maps_array) {
                auto map_obj = map_json.as_object();
                
                std::string id = map_obj["id"].as_string().c_str();
                std::string name = map_obj["name"].as_string().c_str();
                
                // Count loot types
                size_t loot_types_count = 0;
                extra_data::MapExtraData map_extra;
                
                if (map_obj.contains("lootTypes")) {
                    auto& loot_array = map_obj["lootTypes"].as_array();
                    loot_types_count = loot_array.size();
                    
                    // Store extra data
                    extra_data::MapExtraData::LootTypes loot_types;
                    for (const auto& loot : loot_array) {
                        extra_data::LootTypeInfo info;
                        info.data = loot.as_object();
                        loot_types.push_back(std::move(info));
                    }
                    map_extra.SetLootTypes(std::move(loot_types));
                }
                
                auto map = std::make_shared<model::Map>(
                    model::Map::Id(id), name, loot_types_count
                );
                
                // Load roads
                if (map_obj.contains("roads")) {
                    for (const auto& road_json : map_obj["roads"].as_array()) {
                        auto road_obj = road_json.as_object();
                        model::Point start, end;
                        start.x = road_obj["x0"].as_double();
                        start.y = road_obj["y0"].as_double();
                        
                        if (road_obj.contains("x1")) {
                            end.x = road_obj["x1"].as_double();
                            end.y = start.y;
                        } else {
                            end.x = start.x;
                            end.y = road_obj["y1"].as_double();
                        }
                        map->AddRoad({start, end});
                    }
                }
                
                // Load buildings
                if (map_obj.contains("buildings")) {
                    for (const auto& building_json : map_obj["buildings"].as_array()) {
                        auto building_obj = building_json.as_object();
                        model::Rectangle bounds{
                            {building_obj["x"].as_double(), building_obj["y"].as_double()},
                            {building_obj["w"].as_double(), building_obj["h"].as_double()}
                        };
                        map->AddBuilding({bounds});
                    }
                }
                
                // Load offices
                if (map_obj.contains("offices")) {
                    for (const auto& office_json : map_obj["offices"].as_array()) {
                        auto office_obj = office_json.as_object();
                        model::Office office{
                            model::Office::Id(office_obj["id"].as_string().c_str()),
                            {office_obj["x"].as_double(), office_obj["y"].as_double()},
                            {office_obj["offsetX"].as_double(), office_obj["offsetY"].as_double()}
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
                auto session = std::make_shared<model::GameSession>(
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
        std::cerr << "Usage: " << argv[0] << " <config.json>\n";
        return 1;
    }
    
    try {
        GameServer server(argv[1]);
        server.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}