#include "request_handler.h"

namespace http_handler {

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
    map_obj["roads"] = SerializeRoads(map);
    map_obj["buildings"] = SerializeBuildings(map);
    map_obj["offices"] = SerializeOffices(map);
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