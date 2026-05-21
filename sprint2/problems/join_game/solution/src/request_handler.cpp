#include "request_handler.h"

namespace http_handler {

template <typename Body, typename Allocator, typename Send>
void RequestHandler::operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    HandleRequest(std::move(req), std::forward<Send>(send));
}

template <typename Body, typename Allocator, typename Send>
void RequestHandler::HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    std::string target = std::string(req.target());
    std::string method = std::string(req.method_string());

    auto query_pos = target.find('?');
    if (query_pos != std::string::npos) {
        target = target.substr(0, query_pos);
    }

    // JOIN endpoint
    if (target == "/api/v1/game/join") {
        api_handler_.HandleJoin(std::move(req), std::forward<Send>(send));
        return;
    }

    // PLAYERS endpoint
    if (target == "/api/v1/game/players") {
        api_handler_.HandleGetPlayers(std::move(req), std::forward<Send>(send));
        return;
    }

    // MAPS endpoints
    if (target == "/api/v1/maps") {
        if (method != "GET") {
            SendError(std::move(req), send, http::status::method_not_allowed,
                      "badRequest", "Method not allowed");
            return;
        }
        std::string body = SerializeMaps();
        SendResponse(std::move(req), send, http::status::ok, "application/json", body);
        return;
    }

    // Single map endpoint
    const std::string prefix = "/api/v1/maps/";
    if (target.find(prefix) == 0) {
        if (method != "GET") {
            SendError(std::move(req), send, http::status::method_not_allowed,
                      "badRequest", "Method not allowed");
            return;
        }
        std::string map_id_str = target.substr(prefix.length());
        
        while (!map_id_str.empty() && map_id_str.back() == '/') {
            map_id_str.pop_back();
        }

        auto query_pos = map_id_str.find('?');
        if (query_pos != std::string::npos) {
            map_id_str = map_id_str.substr(0, query_pos);
        }

        model::Map::Id map_id{std::move(map_id_str)};
        const model::Map* map = game_.FindMap(map_id);

        if (!map) {
            SendError(std::move(req), send, http::status::not_found,
                      "mapNotFound", "Map not found");
            return;
        }

        std::string body = SerializeMap(*map);
        SendResponse(std::move(req), send, http::status::ok, "application/json", body);
        return;
    }

    // Not found
    SendError(std::move(req), send, http::status::not_found, "badRequest", "Not found");
}

// Вспомогательные методы отправки ответов
template <typename Body, typename Allocator, typename Send>
void RequestHandler::SendResponse(http::request<Body, http::basic_fields<Allocator>>&& req,
                                   Send&& send,
                                   http::status status,
                                   std::string_view content_type,
                                   std::string_view body) {
    http::response<http::string_body> response(status, req.version());
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.prepare_payload();
    response.keep_alive(req.keep_alive());
    send(std::move(response));
}

template <typename Body, typename Allocator, typename Send>
void RequestHandler::SendError(http::request<Body, http::basic_fields<Allocator>>&& req,
                                Send&& send,
                                http::status status,
                                std::string_view code,
                                std::string_view message) {
    std::string body = boost::json::serialize(
        boost::json::object{
            {"code", code},
            {"message", message}
        });
    
    auto response = SendResponse(std::move(req), send, status, "application/json", body);
    response.set(http::field::cache_control, "no-cache");
}

// Методы сериализации остаются без изменений
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