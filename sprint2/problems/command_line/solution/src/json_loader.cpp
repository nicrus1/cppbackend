#include "json_loader.h"
#include <fstream>
#include <sstream>
#include <boost/json.hpp>

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path) {
    // Загрузить содержимое файла json_path
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + json_path.string());
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_str = buffer.str();
    
    // Распарсить строку как JSON
    boost::json::value json_value = boost::json::parse(json_str);
    boost::json::object json_obj = json_value.as_object();
    
    model::Game game;
    
    // Проверяем наличие поля "maps"
    if (!json_obj.contains("maps")) {
        throw std::runtime_error("Config file missing 'maps' field");
    }
    
    const auto& maps_array = json_obj.at("maps").as_array();
    
    // Загружаем defaultDogSpeed
    double default_dog_speed = 1.0;
    if (json_obj.contains("defaultDogSpeed")) {
        default_dog_speed = json_obj.at("defaultDogSpeed").as_double();
    }
    
    // Загружаем каждую карту
    for (const auto& map_value : maps_array) {
        const auto& map_obj = map_value.as_object();
        
        // Читаем id и name
        std::string id = map_obj.at("id").as_string().c_str();
        std::string name = map_obj.at("name").as_string().c_str();
        
        model::Map map(model::Map::Id{std::move(id)}, std::move(name));
        
        // Устанавливаем скорость по умолчанию для карты
        map.SetDefaultDogSpeed(default_dog_speed);
        
        // Загружаем dogSpeed для карты, если есть
        if (map_obj.contains("dogSpeed")) {
            map.SetDogSpeed(map_obj.at("dogSpeed").as_double());
        }
        
        // Загружаем дороги
        if (map_obj.contains("roads")) {
            const auto& roads_array = map_obj.at("roads").as_array();
            for (const auto& road_value : roads_array) {
                const auto& road_obj = road_value.as_object();
                
                int x0 = road_obj.at("x0").as_int64();
                int y0 = road_obj.at("y0").as_int64();
                
                if (road_obj.contains("x1")) {
                    // Горизонтальная дорога
                    int x1 = road_obj.at("x1").as_int64();
                    map.AddRoad(model::Road(model::Road::HORIZONTAL, 
                                           model::Point{x0, y0}, x1));
                } else if (road_obj.contains("y1")) {
                    // Вертикальная дорога
                    int y1 = road_obj.at("y1").as_int64();
                    map.AddRoad(model::Road(model::Road::VERTICAL,
                                           model::Point{x0, y0}, y1));
                }
            }
        }
        
        // Загружаем здания
        if (map_obj.contains("buildings")) {
            const auto& buildings_array = map_obj.at("buildings").as_array();
            for (const auto& building_value : buildings_array) {
                const auto& building_obj = building_value.as_object();
                
                int x = building_obj.at("x").as_int64();
                int y = building_obj.at("y").as_int64();
                int w = building_obj.at("w").as_int64();
                int h = building_obj.at("h").as_int64();
                
                model::Rectangle rect{model::Point{x, y}, model::Size{w, h}};
                map.AddBuilding(model::Building(rect));
            }
        }
        
        // Загружаем офисы
        if (map_obj.contains("offices")) {
            const auto& offices_array = map_obj.at("offices").as_array();
            for (const auto& office_value : offices_array) {
                const auto& office_obj = office_value.as_object();
                
                std::string office_id = office_obj.at("id").as_string().c_str();
                int x = office_obj.at("x").as_int64();
                int y = office_obj.at("y").as_int64();
                int offset_x = office_obj.at("offsetX").as_int64();
                int offset_y = office_obj.at("offsetY").as_int64();
                
                map.AddOffice(model::Office(
                    model::Office::Id{std::move(office_id)},
                    model::Point{x, y},
                    model::Offset{offset_x, offset_y}
                ));
            }
        }
        
        game.AddMap(std::move(map));
    }
    
    return game;
}

}  // namespace json_loader
}